# HUST-Tracker-3576

基于 RK3576 嵌入式平台的**云台智能跟踪系统**。系统通过摄像头采集画面，使用 YOLO v5s 进行实时目标检测（人/车/无人机），选定目标后由 KCF 算法持续跟踪，并驱动伺服云台自动跟随目标运动，使目标始终保持在画面中心。同时通过 RTSP 实时推流和 TCP 远程控制，实现远程监控与操控。

## 功能特性

- **实时目标检测**：YOLO v5s 通过 RKNN NPU 硬件加速，支持 person / car / drone 三类目标
- **单目标跟踪**：KCF 相关滤波 + Kalman 滤波预测，支持坐标点击、ID 指定、运动速度排序三种目标选择方式
- **伺服云台自动跟随**：使用 `servo_vision_track()` 原生视觉跟踪协议，将像素脱靶量发送给伺服，由伺服内部 PID 控制器驱动电机，实现丝滑的云台跟随
- **RTSP 实时推流**：H.264/H.265 编码，可用 VLC 或配套客户端拉流查看
- **TCP 远程控制**：端口 9000，支持目标选择、云台归中、角度设置、锁定模式等指令
- **配套 Windows 客户端**：Qt 应用 [HustTrackerClient](https://github.com/Zhanghaohao666/HustTrackerClient)，提供视频显示、鼠标点击选定目标、云台操控等功能

## 系统架构

```
Camera → VI (YUV420SP) → VPSS (RGB888) → YOLO 检测 / KCF 跟踪
                                              │
                                              ▼
                                    tracker_bridge.c
                                     ├── servo_vision_track() → 伺服云台自动跟随
                                     └── TCP 状态上报 → Windows 客户端
                                              │
                                              ▼
                                    VENC (H.264) → RTSP 推流
```

## 目录结构

```
hust-tracker-3576/
├── Makefile / config.mk             # 构建系统（交叉编译配置）
├── src/
│   ├── sample_huake.c               # 主入口：媒体管线 + 伺服初始化 + TCP 命令处理
│   ├── tracker/
│   │   ├── runtracker.cpp/hpp       # 核心跟踪逻辑：目标选择、状态管理
│   │   ├── tracker_bridge.c/h       # 桥接层：跟踪结果 → 伺服控制 + TCP 上报
│   │   └── draw_id.hpp              # 可视化绘制（检测框/跟踪框/ID）
│   ├── net/
│   │   ├── cmd_protocol.h           # TCP 命令协议定义（设备/客户端共享）
│   │   └── tcp_cmd_server.c/h       # TCP 命令服务器实现
│   ├── include/                     # 跟踪库 API 头文件
│   ├── common/                      # 平台抽象（RTSP、ISP、缓冲区管理）
│   └── model/                       # RKNN 模型文件 + 标签
├── tools/
│   ├── servo_demo.c                 # 伺服云台独立测试工具
│   └── Makefile.servo_demo          # 测试工具编译脚本
├── RK3576_Glibc200_Libs/            # RK3576 平台 SDK 库（从 Release 下载）
├── RK3576_ThirdLibs/                # OpenCV / FFmpeg / libservo 等第三方库（从 Release 下载）
├── src/libs/                        # libTrackerLib.a 预编译跟踪库（从 Release 下载）
└── target/                          # 编译输出目录
```

## 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/Zhanghaohao666/hust-tracker-3576.git
cd hust-tracker-3576
```

### 2. 下载依赖库

平台 SDK 和第三方库体积较大，托管在 [GitHub Release v1.0](https://github.com/Zhanghaohao666/hust-tracker-3576/releases/tag/v1.0) 中。

```bash
# 从 Release 页面下载 hust-tracker-3576-libs.tar.gz (114M)，然后解压到项目根目录：
tar xzf hust-tracker-3576-libs.tar.gz
```

解压后目录结构：
- `RK3576_Glibc200_Libs/` — RK3576 平台 SDK（rkaiq ISP + rockit 媒体框架）
- `RK3576_ThirdLibs/` — OpenCV 4.x / FFmpeg / libservo 等第三方库
- `src/libs/` — libTrackerLib.a 预编译跟踪库（由 [rk3568-tracker-lib](https://github.com/Zhanghaohao666/rk3568-tracker-lib) 编译产出）

### 3. 编译

需要交叉编译工具链 `aarch64-none-linux-gnu-gcc/g++`（需加入 PATH）。

```bash
# 编译主程序
make                    # 输出 target/hust_tracker_3576

# 编译伺服测试工具（可选）
make -f tools/Makefile.servo_demo    # 输出 target/servo_demo

# 清理
make distclean
```

### 4. 部署运行

将 `target/hust_tracker_3576` 和运行时依赖库拷贝到 RK3576 板子上：

```bash
# 运行主程序（自动启动摄像头采集、YOLO 检测、RTSP 推流、TCP 服务器）
./hust_tracker_3576
```

启动后：
- **RTSP 地址**：`rtsp://<板子IP>:554/live`，可用 VLC 打开查看实时画面
- **TCP 控制端口**：9000，可用配套 Windows 客户端连接

### 5. 伺服云台测试（可选）

`servo_demo` 是独立的云台测试工具，无需摄像头和跟踪库，用于验证云台硬件连接：

```bash
./servo_demo center                 # 云台归中
./servo_demo angle <pitch> <yaw>    # 指定角度（度）
./servo_demo speed <err_x> <err_y>  # 手动速度控制测试
./servo_demo vtrack <err_x> <err_y> # 视觉跟踪接口测试
./servo_demo vtrack_all             # 视觉跟踪完整测试（自动测试 4 个方向）
```

## 伺服云台控制

使用伺服原生 `servo_vision_track()` 视觉跟踪协议（消息 ID: 0x0011），将像素脱靶量直接发送给伺服，由伺服内部 PID 控制器驱动电机：

```c
// 跟踪中: 告诉伺服目标偏离画面中心多少像素
// en=2: 跟踪状态, flag=1: 跟踪成功
servo_vision_track(err_x, err_y, 2, 1, box_w, box_h);

// 跟踪丢失: 解锁伺服
servo_vision_track(0, 0, 0, 0, 0, 0);
```

协议参数说明：

| 字段 | 含义 | 值 |
|------|------|---|
| err_x (int16) | 水平脱靶量（像素），向右为正 | 目标中心X - 画面中心X |
| err_y (int16) | 垂直脱靶量（像素），向下为正 | 目标中心Y - 画面中心Y |
| en (uint8) | 跟踪状态 | 0=解锁, 1=检测, **2=跟踪** |
| flag (uint8) | 反馈状态 | 0=失败, **1=成功** |
| box_w/h (uint16) | 目标框宽高 | 像素值 |

## TCP 命令协议

端口：**9000**，协议格式：`[magic=0xAA][cmd_id][payload_len:uint16][payload...]`

| 方向 | cmd_id | 命令 | 说明 |
|------|--------|------|------|
| Client→Device | 0x01 | TRACK_XY | 点击坐标选定目标 |
| Client→Device | 0x02 | TRACK_ID | 按 ID 选定目标 |
| Client→Device | 0x03 | TRACK_UNLOCK | 解锁/停止跟踪 |
| Client→Device | 0x10 | GIMBAL_CENTER | 云台归中 |
| Client→Device | 0x11 | GIMBAL_DOWN90 | 云台俯视 90° |
| Client→Device | 0x12 | GIMBAL_SET_ANGLE | 设置云台角度 |
| Client→Device | 0x13 | GIMBAL_SPEED | 手动速度控制 |
| Client→Device | 0x14 | GIMBAL_LOCK | 锁定/解锁云台 |
| Device→Client | 0x80 | STATUS_REPORT | 跟踪状态 + 目标框 + 云台角度 |
| Device→Client | 0x81 | TARGET_LIST | 检测到的目标列表 |

## 关联项目

| 项目 | 说明 |
|------|------|
| [rk3568-tracker-lib](https://github.com/Zhanghaohao666/rk3568-tracker-lib) | 目标跟踪算法库源码（YOLOv5+KCF+SORT），编译产出 `libTrackerLib.a` |
| [HustTrackerClient](https://github.com/Zhanghaohao666/HustTrackerClient) | 配套 Windows Qt 客户端，RTSP 拉流 + TCP 远程控制 |

## 技术参数

| 项目 | 参数 |
|------|------|
| 平台 | RK3576 (ARM aarch64) |
| 分辨率 | 1920×1080 @ 30fps |
| 输入格式 | YUV420SP → RGB888 |
| 检测模型 | YOLO v5s (RKNN NPU 加速) |
| 检测类别 | person, car, drone |
| 跟踪算法 | KCF + Kalman |
| 伺服通信 | UART 串口, servo_vision_track 视觉跟踪协议 |
| 视频输出 | RTSP (H.264/H.265) |
| 远程控制 | TCP 端口 9000 |
| 交叉编译 | aarch64-none-linux-gnu-gcc, -O3 优化 |
