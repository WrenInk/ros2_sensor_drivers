#!/usr/bin/env python3
"""TCP扫描：连接设备、发送控制命令、打印有应答的IP。所有参数在下面修改。"""
import ipaddress
import json
import socket
import subprocess
from concurrent.futures import ThreadPoolExecutor
from itertools import islice

# ===================== 在这里修改参数 =====================
INTERFACE = "enp8s0"              # 雷达网卡
SOURCE_IP = ""                      # 本机IP；留空自动读取该网卡的第一个IPv4
NETWORK = "192.168.0.0/16"          # 搜索范围；单个IP可写192.168.1.11/32
PORTS = [2110]                      # 需要尝试的TCP端口
CONNECT_TIMEOUT = 0.35              # TCP连接超时，秒
REPLY_TIMEOUT = 2.0                 # 发送命令/等待应答超时，秒
WORKERS = 64                        # 并发数量
RECV_BYTES = 4096                   # 最多读取的应答字节数
PROGRESS_EVERY = 4096               # 每检查多少个IP/端口打印一次进度
TEMPORARY_ROUTE = True              # 跨网段时临时增加直连路由，需要sudo
ROUTE_METRIC = 42760                # 本次临时路由标记
COMMAND = bytes.fromhex(            # WLR716控制命令；其他型号替换为厂家命令
    "FF AA 00 1E 00 00 00 00 00 00 01 01 00 05 00 00 "
    "00 00 00 00 00 00 05 01 00 00 00 00 00 00 00 1F EE EE"
)
# =========================================================


def make_socket(source):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, INTERFACE.encode() + b"\0")
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_DONTROUTE, 1)
        sock.bind((source, 0))
        return sock
    except OSError:
        sock.close()
        raise


def scan(target, source):
    ip, port = target
    try:
        with make_socket(source) as sock:
            sock.settimeout(CONNECT_TIMEOUT)
            sock.connect((ip, port))
            sock.settimeout(REPLY_TIMEOUT)
            sock.sendall(COMMAND)
            try:
                data = sock.recv(RECV_BYTES)
            except socket.timeout:
                data = b""
            print(f"IP={ip} 端口={port} 命令已发送 应答={len(data)}字节", flush=True)
            return ip if data else None
    except OSError:
        return None


def main():
    info = json.loads(subprocess.check_output(["ip", "-j", "-4", "addr", "show", "dev", INTERFACE]))
    addresses = [ipaddress.ip_interface(f"{a['local']}/{a['prefixlen']}")
                 for item in info for a in item.get("addr_info", []) if a["family"] == "inet"]
    if not addresses:
        raise ValueError(f"{INTERFACE} 没有IPv4地址，请先设置本机网口IP")
    source = SOURCE_IP or str(addresses[0].ip)
    local_ips = {str(a.ip) for a in addresses}
    if source not in local_ips:
        raise ValueError(f"SOURCE_IP={source} 不是该网卡已有的IP")
    network = ipaddress.IPv4Network(NETWORK)
    if not COMMAND or not PORTS or any(not 1 <= p <= 65535 for p in PORTS):
        raise ValueError("请检查COMMAND和PORTS")
    if WORKERS < 1 or min(CONNECT_TIMEOUT, REPLY_TIMEOUT) <= 0 or RECV_BYTES < 1 or PROGRESS_EVERY < 1:
        raise ValueError("并发数、超时、接收字节数和进度间隔必须大于0")
    with make_socket(source):  # 先检查网卡绑定权限，避免把权限问题误报为没有设备。
        pass
    route_added = False
    route = [str(network), "dev", INTERFACE, "metric", str(ROUTE_METRIC)]
    found = set()
    try:
        if TEMPORARY_ROUTE and not any(network.subnet_of(a.network) for a in addresses):
            subprocess.run(["ip", "route", "add", *route], check=True)
            route_added = True
        print(f"网卡={INTERFACE} 本机={source} 范围={network} TCP端口={PORTS}", flush=True)
        targets = ((str(ip), port) for ip in network.hosts() if str(ip) not in local_ips for port in PORTS)
        checked = 0
        with ThreadPoolExecutor(max_workers=WORKERS) as pool:
            while batch := list(islice(targets, WORKERS)):
                for result in pool.map(lambda target: scan(target, source), batch):
                    if result:
                        found.add(result)
                checked += len(batch)
                if checked % PROGRESS_EVERY == 0:
                    print(f"已检查 {checked} 个IP/端口组合", flush=True)
    except KeyboardInterrupt:
        print("\n已停止扫描")
    finally:
        if route_added:
            subprocess.run(["ip", "route", "del", *route], check=True)
        print("有应答的设备IP：", ", ".join(sorted(found, key=ipaddress.ip_address)) or "未发现")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"运行失败：{error}；权限不足时使用sudo运行。")
