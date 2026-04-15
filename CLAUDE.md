# HUST-Tracker-3576 项目架构

## 概述

RK3576 嵌入式平台上的云台跟踪系统：YOLO v5s 目标检测 + KCF 相关滤波跟踪，通过 RTSP 输出视频流。

- **平台**: RK3576 (ARM aarch64)，交叉编译工具链 `aarch64-none-linux-gnu-gcc/g++`
- **分辨率**: 1920x1080 @ 30fps，输入 YUV420SP，处理用 RGB888
- **检测类别**: person / car / drone（3类）
- **跟踪**: 单目标 KCF + Kalman 滤波

## 目录结构

```
hust-tracker-3576/
├── Makefile / config.mk          # 构建系统（交叉编译配置）
├── src/
│   ├── sample_huake.c            # 主入口：RK 媒体管线初始化 (VI→VPSS→VENC→RTSP)
│   ├── tracker/
│   │   ├── runtracker.cpp/hpp    # 核心跟踪逻辑：processRGBData()，目标选择，状态管理
│   │   └── draw_id.hpp           # 可视化绘制（检测框/跟踪框/ID）
│   ├── include/
│   │   ├── Tracker_api.hpp       # libTrackerLib 库 API 接口
│   │   ├── dynamic.hpp           # 光流 + 速度排序算法
│   │   └── WF_PIXEL_FORMAT_E.hpp # 像素格式枚举
│   ├── opencv_wrapper.c/h        # NV12↔RGB 转换封装
│   ├── common/                   # 平台抽象（RTSP、ISP、缓冲区管理）
│   ├── model/                    # RKNN 模型文件 (yolov5s *.rknn) + 标签
│   └── libs/
│       ├── libTrackerLib.a       # 预编译跟踪库（KCF/YOLO/Kalman/状态机）
│       └── rknn/                 # RKNN Runtime + RGA + jpeg_turbo
├── RK3576_Glibc200_Libs/         # RK3576 平台库（rkaiq ISP + rockit 媒体框架）
├── RK3576_ThirdLibs/             # OpenCV 4.x + FFmpeg/libav
├── obj/                          # 编译中间文件
└── target/
    └── hust_tracker_3576         # 最终可执行文件
```

## 核心数据流

```
Camera → VI (YUV420SP) → VPSS (RGB888, resize) → processRGBData()
                                                       │
                            ┌──────────────────────────┘
                            ▼
                   Tracker_CVMat_UpdateStatus(mat, w, h, callback)
                            │
                   ┌────────┴────────┐
                   ▼                 ▼
            Status 2: YOLO      Status 3: KCF
            全图检测              单目标跟踪
                   │                 │
                   ▼                 ▼
            drawBoxesByStatus() → 叠加可视化
                            │
                            ▼
                   VENC (H.264/H.265) → RTSP 推流
```

## 跟踪状态机

| 状态 | 值 | 含义 |
|------|---|------|
| WAIT | 1 | 空闲，等待 LOCK 指令 |
| SEARCH | 2 | YOLO 检测中，输出 ntargetnum 个目标 |
| TRACK | 3 | KCF 跟踪中，输出单目标 (xmin,ymin,w,h) |
| RECAP | 4 | 预测性重捕获 |
| LOST | 5 | 跟踪丢失 |

状态 3→非3 转换时触发 `ResetAllStatesOnTrackLost()`，清空所有历史数据并发送 UNLOCK。

## 目标选择模式 (select_flag)

| 模式 | 值 | 说明 |
|------|---|------|
| 速度排序 | 0 | 光流法计算运动速度，锁定最快目标（需200帧预热） |
| 坐标指定 | 1 | 锁定包含 (Coordinate_X, Coordinate_Y) 的检测框 |
| ID指定 | 2 | 锁定 track_id == yolo_num_id 的目标（默认模式） |

## 关键数据结构

- `Object_Property`: 单个检测目标 {xmin, ymin, w, h, score, classId, track_id, ...}
- `Tracker_Objects`: 帧级结果 {frame_id, nTrackerStatus, ntargetnum, Objects[]}
- `YoloBox` / `YoloBox_norm`: 像素坐标 / 归一化坐标的检测框
- `Tracker_Cmd`: 控制指令 {nCmd=LOCK/UNLOCK, 锁定区域坐标}

## 帧处理时序

- Frame 0: `Tracker_Register()` 初始化库
- Frame 1: `Tracker_SetCmd(UNLOCK)` 进入检测模式
- Frame 5+: 根据 select_flag 选择目标，发送 `LOCK` 指令
- 跟踪丢失后自动重置所有状态

## 线程模型

1. **GetMediaBuffer0()**: 从 VPSS 取帧 → `processRGBData()` 处理
2. **venc_get_stream()**: 编码并 RTSP 推流
3. **主线程**: 监控 `quit` 标志

## 构建

```bash
make          # 交叉编译，输出 target/hust_tracker_3576
make clean    # 清理
```

编译选项: `-Wall -O2 -fPIC -fopenmp`，链接 libTrackerLib / librknnrt / OpenCV / librga / librockit 等。

## 伺服云台集成 (2026-04-15)

通过 `libservo` 库实现伺服云台控制，串口 UART 协议通信。

### 关键 API

| 函数 | 用途 |
|------|------|
| `servo_proto_init(NULL)` | 初始化伺服协议栈 |
| `servo_proto_register_cb(on_servo_rx)` | 注册接收回调 |
| `servo_vision_track(err_x, err_y, en, flag, w, h)` | 视觉跟踪：传入目标与画面中心的像素偏差 |
| `servo_gimbal_ctrl(mode, yaw, pitch)` | 云台控制：归中(0x10)、下视(0x20)等 |
| `servo_set_angle(pitch, yaw, roll)` | 设置绝对角度 |
| `servo_set_lock_mode(mode)` | 锁定/解锁云台 |
| `servo_get_version()` | 获取固件版本 |

### 视觉跟踪逻辑 (`runtracker.cpp`)

当 `nTrackerStatus == 3` (TRACK) 时：
```
err_x = target_center_x - image_center_x
err_y = target_center_y - image_center_y
servo_vision_track(err_x, err_y, 1, 0, target_w, target_h)
```
跟踪丢失时调用 `servo_vision_track(0, 0, 0, 0, 0, 0)` 停止伺服跟随。

### 链接依赖

Makefile LDFLAGS: `-llog -lcommcore -lservo`
运行时需要 `libuart.so.1`（已在 RK3576 设备上部署）。

## TCP 命令服务器 (端口 9000)

设备端运行 TCP 命令服务器，接受 Windows 客户端控制指令。

### 协议格式 (`src/net/cmd_protocol.h`)

```
[magic=0xAA][cmd_id][payload_len:uint16] + [payload...]
```

| 方向 | cmd_id | 命令 | Payload |
|------|--------|------|---------|
| Client→Device | 0x01 | TRACK_XY | int16 x, y |
| Client→Device | 0x02 | TRACK_ID | int32 id |
| Client→Device | 0x03 | TRACK_UNLOCK | — |
| Client→Device | 0x10 | GIMBAL_CENTER | — |
| Client→Device | 0x11 | GIMBAL_DOWN90 | — |
| Client→Device | 0x12 | GIMBAL_SET_ANGLE | int16 pitch, yaw |
| Client→Device | 0x13 | GIMBAL_SPEED | uint8 yaw, pitch |
| Client→Device | 0x14 | GIMBAL_LOCK | uint8 mode |
| Device→Client | 0x80 | STATUS_REPORT | tracker_status + bbox + angles + count |

### 文件

- `src/net/cmd_protocol.h` — 协议定义（设备/客户端共享）
- `src/net/tcp_cmd_server.h/.c` — 单客户端 TCP 服务器（POSIX sockets + pthreads）
- `sample_huake.c: on_tcp_command()` — 命令分发处理

### 线程模型更新

4. **accept_loop**: TCP 命令服务器接受连接线程
5. **client_handler**: 每个客户端连接的读取线程

## Windows 客户端 (HustTrackerClient)

位于 `/mnt/A/yt/HustTrackerClient/`，Qt 5/6 + FFmpeg RTSP 拉流客户端。

- `streamworker.h/.cpp` — QThread RTSP 解码（avformat → avcodec → sws_scale → QImage）
- `cmdclient.h/.cpp` — QTcpSocket TCP 命令客户端
- `videolabel.h/.cpp` — 视频显示 + 鼠标点击坐标映射（letterbox 感知）
- `mainwindow.h/.cpp/.ui` — 主界面：连接控制、目标选择、云台操控、日志

构建: Qt Creator 打开 `HustTrackerClient.pro`，需 FFmpeg DLL 与 exe 同目录。

## 已知限制 / TODO

- 配置参数大部分硬编码，运行时配置文件读取未完善
- 仅支持单目标跟踪
- 跟踪丢失后的 ID 重关联未实现
- Windows 客户端尚需在 Windows + Qt Creator 环境下实际构建测试
