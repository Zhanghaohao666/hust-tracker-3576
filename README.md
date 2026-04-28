# HUST-Tracker-3576

基于 RK3576 嵌入式平台的云台智能跟踪系统：YOLO v5s 目标检测 + KCF 单目标跟踪 + 伺服云台联动，通过 RTSP 输出实时视频流。

## 功能特性

- **目标检测**：YOLO v5s（RKNN 加速），支持 person / car / drone 三类目标
- **单目标跟踪**：KCF 相关滤波 + Kalman 滤波，支持多种选择模式（坐标指定、ID 指定、速度排序）
- **伺服云台控制**：通过 `libservo` 库（UART 协议）实现视觉跟踪闭环，云台自动跟随目标
- **RTSP 推流**：H.264/H.265 编码实时推流，可用 VLC 或配套客户端拉流查看
- **TCP 远程控制**：端口 9000，支持目标选择、云台归中、角度设置、锁定模式等指令
- **Windows 客户端**：配套 Qt 客户端 ([HustTrackerClient](https://github.com/Zhanghaohao666/HustTrackerClient))，支持视频显示、点击选定目标、云台操控

## 系统架构

```
Camera → VI (YUV420SP) → VPSS (RGB888) → YOLO 检测 / KCF 跟踪
                                              │
                                              ▼
                                    tracker_bridge.c
                                     ├── 伺服云台控制 (servo_vision_track)
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
│   │   ├── cmd_protocol.h           # TCP 命令协议定义
│   │   └── tcp_cmd_server.c/h       # TCP 命令服务器
│   ├── include/                     # 跟踪库 API、光流算法等头文件
│   ├── common/                      # 平台抽象（RTSP、ISP、缓冲区管理）
│   └── model/                       # RKNN 模型文件 + 标签
├── tools/
│   ├── servo_demo.c                 # 伺服云台独立测试工具
│   └── Makefile.servo_demo          # 测试工具编译脚本
├── RK3576_Glibc200_Libs/            # RK3576 平台 SDK 库（不入 git）
├── RK3576_ThirdLibs/                # OpenCV / FFmpeg / libservo 等第三方库（不入 git）
└── target/                          # 编译输出目录
```

## 编译

### 前置条件

- 交叉编译工具链：`aarch64-none-linux-gnu-gcc/g++`（需加入 PATH）
- 平台库：`RK3576_Glibc200_Libs/` 和 `RK3576_ThirdLibs/` 放置在项目根目录（体积较大，不纳入 git）

### 编译主程序

```bash
make                    # 交叉编译，输出 target/hust_tracker_3576
make distclean          # 完全清理
```

### 编译伺服测试工具

```bash
make -f tools/Makefile.servo_demo    # 输出 target/servo_demo
```

## 部署与运行

将编译产物和依赖库拷贝到 RK3576 板子上：

```bash
# 主程序
./hust_tracker_3576

# 伺服云台测试（无需摄像头 / 跟踪库）
./servo_demo center                 # 云台归中
./servo_demo angle <pitch> <yaw>    # 指定角度
./servo_demo speed <err_x> <err_y>  # 速度控制测试
./servo_demo vtrack <err_x> <err_y> # 视觉跟踪接口测试
./servo_demo vtrack_all             # 视觉跟踪完整测试（4方向）
```

## TCP 命令协议

端口：**9000**，协议格式：`[0xAA][cmd_id][payload_len:uint16][payload...]`

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

## 伺服云台控制

使用 `servo_vision_track()` 原生视觉跟踪接口，将像素脱靶量直接发送给伺服，由伺服内部 PID 控制电机：

```c
// 跟踪中 (en=2: 跟踪状态, flag=1: 跟踪成功)
servo_vision_track(err_x, err_y, 2, 1, box_w, box_h);

// 跟踪丢失 (en=0: 解锁)
servo_vision_track(0, 0, 0, 0, 0, 0);
```

## 技术参数

| 项目 | 参数 |
|------|------|
| 平台 | RK3576 (ARM aarch64) |
| 分辨率 | 1920×1080 @ 30fps |
| 输入格式 | YUV420SP → RGB888 |
| 检测模型 | YOLO v5s (RKNN) |
| 跟踪算法 | KCF + Kalman |
| 伺服通信 | UART 串口协议 (libservo) |
| 视频输出 | RTSP (H.264/H.265) |
| 远程控制 | TCP 端口 9000 |
