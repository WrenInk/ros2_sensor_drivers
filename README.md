# VanJee WLR-716 ROS 2 驱动

本分支包含 **万集（VanJee）WLR-716** 二维单线激光雷达的 ROS 2 驱动工作区、启动配置与雷达 IP 扫描脚本。

实机网络配置、IP 扫描、编译与 RViz2 操作请看 [WLR-716 实机操作](wlr716实机操作.md)。

## 目录结构

- `src/wj_716_lidar/`：ROS 2 驱动包（`wj_716_lidar`）及用于协议参考的原 ROS 1 源码
- `launch/wlr716.launch.py`、`config/wlr716.rviz`：ROS 2 启动与 RViz2 可视化配置
- `scan_lidar.py`：雷达 IP 扫描脚本
- `wlr716实机操作.md`：完整实机操作文档

## 快速使用（ROS 2 Humble）

```bash
# 1. 克隆本分支作为工作区
git clone -b wlr716 https://github.com/WrenInk/ros2_sensor_drivers.git ~/wlr716
cd ~/wlr716

# 2. 编译驱动
source /opt/ros/humble/setup.bash
colcon build --packages-select wj_716_lidar

# 3. 启动驱动与 RViz2（替换实际的雷达 IP 与端口）
source install/setup.bash
ros2 launch ./launch/wlr716.launch.py hostname:=192.168.10.20 port:=2110
```
