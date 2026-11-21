# ROS 2 传感器驱动集合（Sensor Drivers for ROS 2）

本仓库参考 [Lslidar_ROS2_driver](https://github.com/Lslidar/Lslidar_ROS2_driver) 的多分支组织方式，将不同传感器的驱动按产品名称拆分到各自独立的分支中。`main` 分支仅作为总览目录与快速导航页，各传感器驱动代码相互隔离，按需拉取对应分支即可使用。

---

## 传感器驱动分支目录

点击下方**分支名称**可直接跳转到对应传感器的驱动分支（在 GitHub / Gitee / GitLab 页面中生效）：

| 传感器型号 | 厂商 | 传感器类型 | 通信接口 | ROS 2 包 / 内容 | 驱动分支 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Mid-360** | Livox（览沃） | 3D 固态激光雷达 | 以太网（UDP） | `livox_ros_driver2` + 实机操作文档 | [mid360](../../tree/mid360) |
| **STP-23L** | LDROBOT（乐动） | 单点 ToF 激光测距 | 串口（230400 / 8N1） | `read_stp23l.py` 串口读取脚本 | [stp23l](../../tree/stp23l) |
| **WLR-716** | VanJee（万集） | 2D 单线激光雷达 | 以太网（TCP） | `wj_716_lidar` + launch / rviz / IP 扫描脚本 | [wlr716](../../tree/wlr716) |
| **N10** | Lslidar（镭神智能） | 2D 单线激光雷达 | 串口 | `lslidar_driver` + `lslidar_msgs` | [lslidar_N10](../../tree/lslidar_N10) |

---

## 通用使用方式

### 1. 准备工作空间

推荐使用 Ubuntu 22.04 + **ROS 2 Humble** 环境：

```bash
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src
```

### 2. 按传感器型号拉取对应分支

使用 `git clone -b <分支名>` 仅克隆所需传感器的驱动分支（将 `<仓库地址>` 替换为本仓库实际 URL）：

```bash
git clone -b <分支名> <仓库地址>
```

### 3. 编译与加载环境

```bash
cd ~/ros2_ws
source /opt/ros/humble/setup.bash
colcon build
source install/setup.bash
```

---

## 各传感器快速指引

### Livox Mid-360（分支：[`mid360`](../../tree/mid360)）

- 需要预先安装 `Livox-SDK2` 并配置雷达与主机静态 IP，详见分支内的 `mid360实机操作.md`。
- 拉取与启动：
  ```bash
  git clone -b mid360 <仓库地址> mid360
  cd mid360/livox_ros_driver2 && ./build.sh humble
  ros2 launch livox_ros_driver2 rviz_MID360_launch.py
  ```

### LDROBOT STP-23L（分支：[`stp23l`](../../tree/stp23l)）

- 当前提供基于 `pyserial` 的独立串口距离帧读取脚本 `read_stp23l.py`（`230400 / 8N1`）。
- 拉取与运行：
  ```bash
  git clone -b stp23l <仓库地址> stp23l
  cd stp23l
  python3 read_stp23l.py --port /dev/ttyUSB0
  ```

### VanJee WLR-716（分支：[`wlr716`](../../tree/wlr716)）

- 包含 ROS 2 驱动 `wj_716_lidar`、RViz2 配置及雷达 IP 扫描脚本 `scan_lidar.py`，详见分支内的 `wlr716实机操作.md`。
- 拉取与启动：
  ```bash
  git clone -b wlr716 <仓库地址> ~/wlr716
  cd ~/wlr716
  colcon build --packages-select wj_716_lidar
  source install/setup.bash
  ros2 launch ./launch/wlr716.launch.py hostname:=<雷达IP> port:=<TCP端口>
  ```

### Lslidar N10（分支：[`lslidar_N10`](../../tree/lslidar_N10)）

- 包含 `lslidar_driver` 与 `lslidar_msgs` 两个 ROS 2 功能包，详见分支内的 `README.md`。
- 拉取与启动：
  ```bash
  git clone -b lslidar_N10 <仓库地址> lslidar_N10
  # 在工作空间根目录编译后启动
  colcon build
  source install/setup.bash
  ros2 launch lslidar_driver lslidar_launch.py
  ```

---

## 维护说明：新增传感器驱动分支

如需继续往本仓库添加新的传感器驱动，建议使用独立的孤儿分支（orphan branch）：

```bash
# 1. 创建新的独立分支
git checkout --orphan <传感器产品名>
git rm -rf .

# 2. 放入对应传感器驱动代码、README.md 与 .gitignore，然后提交
git add -A
git commit -m "Add <传感器产品名> ROS 2 driver"

# 3. 切回 main 分支，更新本 README.md 的目录表格与跳转链接
git checkout main
```
