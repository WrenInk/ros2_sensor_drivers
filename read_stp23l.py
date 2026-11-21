#!/usr/bin/env python3
"""读取 STP-23L 的距离帧：python3 read_stp23l.py --help。

串口参数来自 LDROBOT STP-23L Datasheet V1.0：230400 / 8N1。
帧结构参考第三方作者实现，并在本机传感器上验证校验和：
https://github.com/xrobot-org/STP23L/blob/master/STP23L.hpp
"""

import argparse
import csv
from datetime import datetime
from pathlib import Path
import struct
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("缺少 pyserial，请先安装：python3 -m pip install pyserial")


HEADER = bytes.fromhex("aa aa aa aa 00 02 00 00 b8 00")
FRAME_SIZE = 195
POINT = struct.Struct("<hHIBIh")
STABLE_PORT = "/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0"


class FrameParser:
    """缓存不完整的帧，丢弃校验失败的数据，并重新寻找帧头。"""

    def __init__(self):
        self.buffer = bytearray()
        self.bad_checksums = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while True:
            start = self.buffer.find(HEADER)
            if start < 0:
                # 保留可能跨两次读取的帧头前缀。
                self.buffer = self.buffer[-(len(HEADER) - 1):]
                break
            del self.buffer[:start]
            if len(self.buffer) < FRAME_SIZE:
                break
            frame = self.buffer[:FRAME_SIZE]
            if (sum(frame[4:-1]) & 0xFF) != frame[-1]:
                self.bad_checksums += 1
                del self.buffer[0]
                continue
            points = [POINT.unpack_from(frame, 10 + i * POINT.size) for i in range(12)]
            timestamp = struct.unpack_from("<I", frame, 190)[0]
            frames.append((timestamp, points))
            del self.buffer[:FRAME_SIZE]
        return frames


def main():
    parser = argparse.ArgumentParser(description="实时读取 STP-23L 距离（毫米），Ctrl+C 停止。")
    parser.add_argument("--port", default=STABLE_PORT if Path(STABLE_PORT).exists() else "/dev/ttyUSB0",
                        help="串口路径，默认使用本机 CH340 的稳定路径或 /dev/ttyUSB0")
    parser.add_argument("--count", type=int, default=0, help="读取指定帧数后退出；0 表示持续读取")
    parser.add_argument("--all", action="store_true", help="显示每帧全部 12 个原始距离及置信度")
    parser.add_argument("--csv", type=Path, help="保存每个样本的原始字段到新的 CSV 文件")
    args = parser.parse_args()
    if args.count < 0:
        parser.error("--count 不能为负数")

    decoder = FrameParser()
    received = 0
    output = None
    writer = None
    port = None
    try:
        if args.csv:
            # 避免覆盖已有的测量记录。
            output = args.csv.open("x", newline="", encoding="utf-8")
            writer = csv.writer(output)
            writer.writerow(["host_time", "sensor_timestamp_raw", "sample_index", "distance_mm",
                             "noise", "peak", "confidence_raw", "integration", "reference_tof"])
        port = serial.Serial(port=None, baudrate=230400, timeout=0.2, exclusive=True,
                             bytesize=8, parity="N", stopbits=1,
                             xonxoff=False, rtscts=False, dsrdtr=False)
        port.dtr = False
        port.rts = False
        port.port = args.port
        with port:
            print(f"已打开 {args.port}，230400 / 8N1；距离单位 mm，Ctrl+C 停止。", flush=True)
            last_frame = time.monotonic()
            while not args.count or received < args.count:
                chunk = port.read(min(max(port.in_waiting, 1), 4096))
                for timestamp, points in decoder.feed(chunk):
                    last_frame = time.monotonic()
                    received += 1
                    host_time = datetime.now().astimezone().isoformat(timespec="milliseconds")
                    # 仅作显示筛选；非零置信度不等同于厂家精度保证。
                    distances = [p[0] for p in points if p[0] > 0 and p[3] > 0]
                    if distances:
                        average = sum(distances) / len(distances)
                        message = (f"平均 {average:8.1f} mm ({average / 1000:.3f} m)"
                                   f"  范围 {min(distances)}–{max(distances)} mm"
                                   f"  正距离且置信度非零 {len(distances)}/12")
                    else:
                        message = "本帧没有正距离且置信度非零的样本"
                    if args.all:
                        message += f"  距离={[p[0] for p in points]}  置信度={[p[3] for p in points]}"
                    print(f"{host_time}  {message}", flush=True)
                    if writer:
                        for index, point in enumerate(points):
                            writer.writerow([host_time, timestamp, index, *point])
                        output.flush()
                    if args.count and received >= args.count:
                        break
                if time.monotonic() - last_frame > 3:
                    print("连续 3 秒未收到通过校验的距离帧；请检查供电、TX→RX 接线和串口占用。",
                          file=sys.stderr)
                    return 1
    except KeyboardInterrupt:
        print("\n已停止。")
    except (OSError, serial.SerialException) as error:
        print(f"读取失败：{error}", file=sys.stderr)
        return 1
    finally:
        if port is not None:
            port.close()
        if output:
            output.close()
        print(f"共读取 {received} 帧，校验失败 {decoder.bad_checksums} 帧。", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
