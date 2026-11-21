# Livox Mid-360 ROS 2 驱动

本分支包含 **Livox Mid-360** 3D 激光雷达的 ROS 2 驱动源码（`livox_ros_driver2`）及实机操作文档。

## 目录结构

- `livox_ros_driver2/`：Livox 官方 ROS 2 驱动源码包
- `mid360实机操作.md`：有线网络配置、Livox-SDK2 安装、参数配置、编译及 RViz2 可视化完整指南

## 快速使用（ROS 2 Humble）

> 前置依赖：请先按照 [mid360实机操作.md](mid360实机操作.md) 安装底层通信库 `Livox-SDK2` 并配置静态 IP。

```bash
# 1. 克隆本分支到 ROS 2 工作空间的 src 目录
mkdir -p ~/mid360_ws/src
cd ~/mid360_ws/src
git clone -b mid360 <仓库地址> mid360

# 2. 编译驱动
cd ~/mid360_ws/src/mid360/livox_ros_driver2
source /opt/ros/humble/setup.bash
./build.sh humble

# 3. 启动驱动与 RViz2 可视化
cd ~/mid360_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch livox_ros_driver2 rviz_MID360_launch.py
```

详细配置与常见问题排查请参阅 [mid360实机操作.md](mid360实机操作.md) 及 [livox_ros_driver2/README.md](livox_ros_driver2/README.md)。
