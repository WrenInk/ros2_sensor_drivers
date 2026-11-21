#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <cmath>
#include <chrono>

using namespace std::chrono_literals;

class WLR716DriverNode : public rclcpp::Node
{
public:
  WLR716DriverNode() : Node("wj_716_lidar")
  {
    // Declare parameters
    this->declare_parameter<std::string>("hostname", "192.168.0.2");
    this->declare_parameter<int>("port", 2110);
    this->declare_parameter<std::string>("frame_id", "laser");
    this->declare_parameter<std::string>("scan_topic", "scan");
    this->declare_parameter<double>("min_ang", -2.35619449);   // -135 deg
    this->declare_parameter<double>("max_ang", 2.35619449);    // +135 deg
    this->declare_parameter<double>("angle_increment", 0.005817764); // 270 deg / 810 intervals
    this->declare_parameter<double>("time_increment", 0.000123);
    this->declare_parameter<double>("range_min", 0.05);
    this->declare_parameter<double>("range_max", 25.0);
    this->declare_parameter<int>("resize", 811);

    hostname_ = this->get_parameter("hostname").as_string();
    port_ = this->get_parameter("port").as_int();
    frame_id_ = this->get_parameter("frame_id").as_string();
    scan_topic_ = this->get_parameter("scan_topic").as_string();
    min_ang_ = this->get_parameter("min_ang").as_double();
    max_ang_ = this->get_parameter("max_ang").as_double();
    angle_increment_ = this->get_parameter("angle_increment").as_double();
    time_increment_ = this->get_parameter("time_increment").as_double();
    range_min_ = this->get_parameter("range_min").as_double();
    range_max_ = this->get_parameter("range_max").as_double();
    total_points_ = this->get_parameter("resize").as_int();

    RCLCPP_INFO(this->get_logger(), "WLR-716 Driver Config:");
    RCLCPP_INFO(this->get_logger(), "  Target LiDAR: %s:%d", hostname_.c_str(), port_);
    RCLCPP_INFO(this->get_logger(), "  Topic: %s, Frame: %s", scan_topic_.c_str(), frame_id_.c_str());
    RCLCPP_INFO(this->get_logger(), "  FOV: [%.2f rad, %.2f rad], Points: %zu", min_ang_, max_ang_, total_points_);

    laser_pub_ = this->create_publisher<sensor_msgs::msg::LaserScan>(scan_topic_, 10);

    scan_ranges_.resize(total_points_, std::numeric_limits<float>::quiet_NaN());

    sockfd_ = -1;
    running_ = true;
    connected_ = false;

    // Heartbeat timer (sends mode command every 2.0s)
    heartbeat_timer_ = this->create_wall_timer(
      2000ms, std::bind(&WLR716DriverNode::sendHeartbeat, this));

    // Start worker thread for socket I/O
    worker_thread_ = std::thread(&WLR716DriverNode::receiveLoop, this);
  }

  ~WLR716DriverNode()
  {
    running_ = false;
    closeSocket();
    if (worker_thread_.joinable()) {
      worker_thread_.join();
    }
  }

private:
  void closeSocket()
  {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    connected_ = false;
    if (sockfd_ >= 0) {
      ::close(sockfd_);
      sockfd_ = -1;
    }
  }

  bool connectToLidar()
  {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (sockfd_ >= 0) {
      ::close(sockfd_);
      sockfd_ = -1;
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      RCLCPP_ERROR(this->get_logger(), "Failed to create TCP socket");
      return false;
    }

    // Set 2s receive timeout
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, hostname_.c_str(), &serv_addr.sin_addr) <= 0) {
      RCLCPP_ERROR(this->get_logger(), "Invalid IP address: %s", hostname_.c_str());
      ::close(fd);
      return false;
    }

    if (::connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      ::close(fd);
      return false;
    }

    sockfd_ = fd;
    connected_ = true;
    RCLCPP_INFO(this->get_logger(), "Successfully connected to WLR-716 at %s:%d!", hostname_.c_str(), port_);

    // Send initial configuration command immediately upon connection
    sendScanModeCommandLocked();
    return true;
  }

  void sendScanModeCommandLocked()
  {
    if (sockfd_ < 0) return;
    const unsigned char scanmodeset1[34] = {
      0xFF, 0xAA, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x01, 0x01, 0x00, 0x05, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f,
      0xEE, 0xEE
    };
    ssize_t sent = ::send(sockfd_, scanmodeset1, sizeof(scanmodeset1), MSG_NOSIGNAL);
    if (sent != sizeof(scanmodeset1)) {
      RCLCPP_WARN(this->get_logger(), "Failed to send scan mode command");
    }
  }

  void sendHeartbeat()
  {
    std::lock_guard<std::mutex> lock(socket_mutex_);
    if (!connected_ || sockfd_ < 0) {
      return;
    }
    sendScanModeCommandLocked();
  }

  void receiveLoop()
  {
    std::vector<uint8_t> buffer;
    buffer.reserve(32768);
    uint8_t recv_buf[4096];

    while (running_ && rclcpp::ok()) {
      if (!connected_) {
        RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
          "Connecting to WLR-716 at %s:%d...", hostname_.c_str(), port_);
        if (!connectToLidar()) {
          std::this_thread::sleep_for(1000ms);
          continue;
        }
      }

      int current_fd = -1;
      {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        current_fd = sockfd_;
      }
      if (current_fd < 0) {
        continue;
      }

      ssize_t n = ::recv(current_fd, recv_buf, sizeof(recv_buf), 0);
      if (n > 0) {
        buffer.insert(buffer.end(), recv_buf, recv_buf + n);
        processStreamBuffer(buffer);
      } else if (n == 0) {
        RCLCPP_WARN(this->get_logger(), "Connection closed by WLR-716 peer");
        closeSocket();
      } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Timeout, heartbeat will keep it alive or reconnect
          continue;
        } else {
          RCLCPP_WARN(this->get_logger(), "Socket error: %s", strerror(errno));
          closeSocket();
        }
      }
    }
  }

  void processStreamBuffer(std::vector<uint8_t>& buf)
  {
    while (buf.size() >= 4) {
      if (buf[0] == 0xff && buf[1] == 0xaa) {
        size_t len = (static_cast<size_t>(buf[2]) << 8 | buf[3]) + 4;
        if (len > 65536 || len < 4) { buf.erase(buf.begin()); continue; }
        if (buf.size() < len) break;
        buf.erase(buf.begin(), buf.begin() + len);
      } else if (buf[0] == 2 && buf[1] == 2 && buf[2] == 2 && buf[3] == 2) {
        if (buf.size() < 8) break;
        size_t len = (static_cast<size_t>(buf[4]) << 24 | static_cast<size_t>(buf[5]) << 16 |
                      static_cast<size_t>(buf[6]) << 8 | buf[7]) + 9;
        if (len < 86 || len > 65536) { buf.erase(buf.begin()); continue; }
        if (buf.size() < len) break;
        uint8_t check = 0;
        for (size_t i = 8; i + 1 < len; ++i) check ^= buf[i];
        if (check == buf[len - 1]) parseFramePayload(buf.data(), len);
        buf.erase(buf.begin(), buf.begin() + len);
      } else {
        buf.erase(buf.begin());
      }
    }
  }

  void parseFramePayload(const uint8_t* frame, size_t len)
  {
    if (len < 86 || frame[8] != 0x73 || (frame[9] != 0x52 && frame[9] != 0x53)) return;
    const unsigned packet = (frame[50] << 8) | frame[51];
    const uint32_t frame_id = (uint32_t(frame[46]) << 24) | (uint32_t(frame[47]) << 16) |
                              (uint32_t(frame[48]) << 8) | frame[49];
    if (packet != 1 && packet != 2) return;
    const size_t count = packet == 1 ? 405 : 406;
    const size_t start = packet == 1 ? 0 : 405;
    if (len < 85 + count * 2 + 1 || scan_ranges_.size() < 811) return;
    if (packet == 1) { frame_id_716_ = frame_id; have_first_ = true; }
    else if (!have_first_ || frame_id != frame_id_716_) return;
    for (size_t i = 0; i < count; ++i) {
      const uint16_t value = (frame[85 + i * 2] << 8) | frame[86 + i * 2];
      const float distance = value / 1000.0f;
      scan_ranges_[start + i] = distance < range_min_ || distance >= range_max_
        ? std::numeric_limits<float>::quiet_NaN() : distance;
    }
    if (packet == 2) { have_first_ = false; publishScanMessage(); }
  }

  void publishScanMessage()
  {
    auto scan_msg = sensor_msgs::msg::LaserScan();
    scan_msg.header.stamp = this->now();
    scan_msg.header.frame_id = frame_id_;
    scan_msg.angle_min = min_ang_;
    scan_msg.angle_max = max_ang_;
    scan_msg.angle_increment = angle_increment_;
    scan_msg.time_increment = time_increment_;
    scan_msg.scan_time = 0.0f; // scan frequency is device dependent
    scan_msg.range_min = range_min_;
    scan_msg.range_max = range_max_;
    scan_msg.ranges = scan_ranges_;

    laser_pub_->publish(scan_msg);
  }

  // Configuration
  std::string hostname_;
  int port_;
  std::string frame_id_;
  std::string scan_topic_;
  double min_ang_;
  double max_ang_;
  double angle_increment_;
  double time_increment_;
  double range_min_;
  double range_max_;
  size_t total_points_;

  uint32_t frame_id_716_ = 0;
  bool have_first_ = false;

  // State
  int sockfd_;
  std::atomic<bool> running_;
  std::atomic<bool> connected_;
  std::mutex socket_mutex_;
  std::thread worker_thread_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  std::vector<float> scan_ranges_;
  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr laser_pub_;
};

int main(int argc, char** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<WLR716DriverNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
