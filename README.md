# LDROBOT STP-23L 驱动

本分支包含 **乐动（LDROBOT）STP-23L** 单点 ToF 激光测距传感器的串口读取驱动脚本。

## 目录结构

- `read_stp23l.py`：STP-23L 串口距离帧实时解析脚本（波特率 `230400`，`8N1`）

## 依赖安装

```bash
python3 -m pip install pyserial
```

## 快速使用

```bash
# 克隆本分支
git clone -b stp23l <仓库地址> stp23l
cd stp23l

# 查看帮助
python3 read_stp23l.py --help

# 实时读取距离（默认自动检测串口，也可通过 --port 指定）
python3 read_stp23l.py --port /dev/ttyUSB0

# 显示每帧全部 12 个采样点距离与置信度
python3 read_stp23l.py --port /dev/ttyUSB0 --all

# 保存测量数据到 CSV
python3 read_stp23l.py --port /dev/ttyUSB0 --csv output.csv
```

> 提示：当前分支为独立的 Python 串口读取脚本，后续可直接基于 `FrameParser` 封装为 ROS 2 `sensor_msgs/msg/Range` 发布节点。
