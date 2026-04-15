# Servo Control Integration + RTSP Streaming Client Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate servo gimbal control into the RK3576 tracker so the gimbal follows tracked targets, and build a Windows RTSP streaming client with click-to-track, ID-based target selection, and gimbal control UI.

**Architecture:** Two sub-projects sharing a TCP command protocol. Sub-project A (device-side) adds servo libraries, a TCP command server, and vision-tracking servo calls to the existing RK3576 tracker. Sub-project B (client-side) is a simplified Qt application based on SampleTool's FFmpeg RTSP streaming, with added mouse click tracking, ID selection, and servo control panels. The client connects to the device's RTSP stream (video) and TCP command port (control) simultaneously.

**Tech Stack:**
- Device: C/C++ on RK3576 (aarch64), libservo, POSIX sockets
- Client: Qt 5/6 + C++ on Windows, FFmpeg 7.0 (from SampleTool), TCP sockets

---

## Sub-Project A: Device-Side (RK3576 Servo Integration + TCP Command Server)

### Overview

```
                        +-------------------------------------+
  Windows Client        |         RK3576 Device               |
  +----------+   RTSP   |  Camera->VI->VPSS->processRGBData() |
  | Video    |<---------|----->VENC->RTSP Server (:554)        |
  | Display  |          |           |                          |
  +----------+   TCP    |     +-----v------+                  |
  | Control  |--------->|-----|TCP Server  |                  |
  | Panel    |  (:9000) |     |  (:9000)   |                  |
  +----------+          |     +-----+------+                  |
                        |           | commands                 |
                        |     +-----v------+    +----------+  |
                        |     | runtracker |--->| libservo |  |
                        |     |  .cpp      |    | (UART)   |  |
                        |     +------------+    +----------+  |
                        +-------------------------------------+
```

### TCP Command Protocol

Client to Device commands are simple fixed-size binary packets:

```c
// All packets: 4-byte header + payload
typedef struct {
    uint8_t  magic;      // 0xAA
    uint8_t  cmd_id;     // Command type
    uint16_t payload_len; // Payload bytes (little-endian)
    // followed by payload_len bytes
} cmd_header_t;

// cmd_id values:
// CMD_TRACK_XY        0x01  payload: int16_t x, int16_t y (pixel coords in 1920x1080)
// CMD_TRACK_ID        0x02  payload: int32_t target_id
// CMD_TRACK_UNLOCK    0x03  payload: none
// CMD_GIMBAL_CENTER   0x10  payload: none
// CMD_GIMBAL_DOWN90   0x11  payload: none
// CMD_GIMBAL_SET_ANGLE 0x12 payload: int16_t pitch_deg, int16_t yaw_deg
// CMD_GIMBAL_SPEED    0x13  payload: uint8_t yaw_speed (0-255, 128=stop), uint8_t pitch_speed
// CMD_GIMBAL_LOCK     0x14  payload: uint8_t mode (0=unlock, 1=lock)

// Device to Client status report (periodic, every frame)
// CMD_STATUS_REPORT   0x80  payload: status_report_t
typedef struct {
    uint8_t  tracker_status;  // 1=WAIT, 2=SEARCH, 3=TRACK, 4=RECAP, 5=LOST
    int16_t  track_x, track_y, track_w, track_h; // current track box
    int16_t  yaw_angle;       // 0.01 deg units
    int16_t  pitch_angle;     // 0.01 deg units
    uint8_t  target_count;    // number of detected targets
} status_report_t;
```

---

### Task A1: Add Servo Libraries and Headers to Project

**Files:**
- Copy: `RK3576_ThirdLibs/include/libservo/` from new sample_huake
- Copy: `RK3576_ThirdLibs/lib/libservo.*`, `libcommcore.*`, `liblog.*` from new sample_huake
- Modify: `hust-tracker-3576/Makefile:38`

- [ ] **Step 1: Copy servo header files**

Copy entire `/mnt/A/yt/_tmp_sample_huake/sample_huake/RK3576_ThirdLibs/include/libservo` directory into `/mnt/A/yt/hust-tracker-3576/RK3576_ThirdLibs/include/`.

- [ ] **Step 2: Copy servo libraries**

Copy from `/mnt/A/yt/_tmp_sample_huake/sample_huake/RK3576_ThirdLibs/lib/`:
- `libservo.a`, `libservo.so`, `libservo.so.1`, `libservo.so.1.0.0`
- `libcommcore.a`, `libcommcore.so`, `libcommcore.so.1`, `libcommcore.so.1.0.0`
- `liblog.a`, `liblog.so`, `liblog.so.1`, `liblog.so.1.0.0`

Into `/mnt/A/yt/hust-tracker-3576/RK3576_ThirdLibs/lib/`.

- [ ] **Step 3: Update Makefile to link servo libraries**

In `hust-tracker-3576/Makefile`, line 38, change:
```makefile
LDFLAGS += -lsample_comm -lrtsp
```
to:
```makefile
LDFLAGS += -lsample_comm -lrtsp -llog -lcommcore -lservo
```

- [ ] **Step 4: Verify the build compiles successfully**

Run: `cd /mnt/A/yt/hust-tracker-3576 && make clean && make`
Expected: Compilation succeeds, `target/hust_tracker_3576` is produced.

- [ ] **Step 5: Commit**

```
git add RK3576_ThirdLibs/include/libservo/ RK3576_ThirdLibs/lib/ Makefile
git commit -m "feat: add servo control libraries and headers"
```

---

### Task A2: Create TCP Command Protocol Header

**Files:**
- Create: `hust-tracker-3576/src/net/cmd_protocol.h`

- [ ] **Step 1: Create the protocol header**

File: `src/net/cmd_protocol.h`

```c
#ifndef CMD_PROTOCOL_H
#define CMD_PROTOCOL_H

#include <stdint.h>

#define CMD_MAGIC           0xAA
#define CMD_PORT            9000

// Client -> Device command IDs
#define CMD_TRACK_XY        0x01
#define CMD_TRACK_ID        0x02
#define CMD_TRACK_UNLOCK    0x03
#define CMD_GIMBAL_CENTER   0x10
#define CMD_GIMBAL_DOWN90   0x11
#define CMD_GIMBAL_SET_ANGLE 0x12
#define CMD_GIMBAL_SPEED    0x13
#define CMD_GIMBAL_LOCK     0x14

// Device -> Client
#define CMD_STATUS_REPORT   0x80

#pragma pack(push, 1)

typedef struct {
    uint8_t  magic;
    uint8_t  cmd_id;
    uint16_t payload_len;
} cmd_header_t;

typedef struct {
    int16_t x;
    int16_t y;
} cmd_track_xy_t;

typedef struct {
    int32_t target_id;
} cmd_track_id_t;

typedef struct {
    int16_t pitch_deg;
    int16_t yaw_deg;
} cmd_gimbal_angle_t;

typedef struct {
    uint8_t yaw_speed;   // 0-255, 128 = stop
    uint8_t pitch_speed;
} cmd_gimbal_speed_t;

typedef struct {
    uint8_t mode; // 0=unlock, 1=lock
} cmd_gimbal_lock_t;

typedef struct {
    uint8_t  tracker_status;
    int16_t  track_x;
    int16_t  track_y;
    int16_t  track_w;
    int16_t  track_h;
    int16_t  yaw_angle;
    int16_t  pitch_angle;
    uint8_t  target_count;
} status_report_t;

#pragma pack(pop)

#endif // CMD_PROTOCOL_H
```

- [ ] **Step 2: Commit**

```
git add src/net/cmd_protocol.h
git commit -m "feat: add TCP command protocol definition"
```

---

### Task A3: Create TCP Command Server

**Files:**
- Create: `hust-tracker-3576/src/net/tcp_cmd_server.h`
- Create: `hust-tracker-3576/src/net/tcp_cmd_server.c`

- [ ] **Step 1: Create TCP server header**

File: `src/net/tcp_cmd_server.h`

```c
#ifndef TCP_CMD_SERVER_H
#define TCP_CMD_SERVER_H

#include "cmd_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cmd_callback_t)(uint8_t cmd_id, const void *payload, uint16_t len);

int tcp_cmd_server_start(cmd_callback_t cb);
void tcp_cmd_server_stop(void);
int tcp_cmd_server_send_status(const status_report_t *report);

#ifdef __cplusplus
}
#endif

#endif
```

- [ ] **Step 2: Create TCP server implementation**

File: `src/net/tcp_cmd_server.c`

Implementation: single-client TCP server on port 9000.
- `tcp_cmd_server_start()`: creates socket, binds, listens, spawns accept thread.
- Accept thread: accepts one client, spawns recv thread, replaces old client.
- Recv thread: reads `cmd_header_t` + payload, calls callback.
- `tcp_cmd_server_send_status()`: sends header + status_report_t to connected client.
- `tcp_cmd_server_stop()`: shuts down sockets, joins threads.

Uses POSIX sockets (`socket`, `bind`, `listen`, `accept`, `recv`, `send`), `pthread_t` for threads, `pthread_mutex_t` for client fd protection.

Full code (~120 lines) as shown in detailed implementation below:

```c
#include "tcp_cmd_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int g_server_fd = -1;
static int g_client_fd = -1;
static pthread_t g_accept_thread;
static volatile int g_running = 0;
static cmd_callback_t g_callback = NULL;
static pthread_mutex_t g_client_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *client_handler(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    uint8_t buf[256];
    while (g_running) {
        cmd_header_t hdr;
        int n = recv(fd, &hdr, sizeof(hdr), MSG_WAITALL);
        if (n <= 0) break;
        if (hdr.magic != CMD_MAGIC) continue;
        if (hdr.payload_len > sizeof(buf)) continue;
        if (hdr.payload_len > 0) {
            n = recv(fd, buf, hdr.payload_len, MSG_WAITALL);
            if (n <= 0) break;
        }
        if (g_callback) g_callback(hdr.cmd_id, buf, hdr.payload_len);
    }
    pthread_mutex_lock(&g_client_mutex);
    if (g_client_fd == fd) g_client_fd = -1;
    pthread_mutex_unlock(&g_client_mutex);
    close(fd);
    return NULL;
}

static void *accept_loop(void *arg) {
    (void)arg;
    while (g_running) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int fd = accept(g_server_fd, (struct sockaddr *)&addr, &len);
        if (fd < 0) { if (g_running) usleep(100000); continue; }
        printf("[TCP] Client connected: %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        pthread_mutex_lock(&g_client_mutex);
        if (g_client_fd >= 0) close(g_client_fd);
        g_client_fd = fd;
        pthread_mutex_unlock(&g_client_mutex);
        pthread_t t;
        int *pfd = (int *)malloc(sizeof(int));
        *pfd = fd;
        pthread_create(&t, NULL, client_handler, pfd);
        pthread_detach(t);
    }
    return NULL;
}

int tcp_cmd_server_start(cmd_callback_t cb) {
    g_callback = cb;
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) return -1;
    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(CMD_PORT);
    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[TCP] bind"); close(g_server_fd); return -1;
    }
    if (listen(g_server_fd, 1) < 0) { close(g_server_fd); return -1; }
    g_running = 1;
    pthread_create(&g_accept_thread, NULL, accept_loop, NULL);
    printf("[TCP] Command server on port %d\n", CMD_PORT);
    return 0;
}

void tcp_cmd_server_stop(void) {
    g_running = 0;
    if (g_server_fd >= 0) { shutdown(g_server_fd, SHUT_RDWR); close(g_server_fd); g_server_fd = -1; }
    pthread_mutex_lock(&g_client_mutex);
    if (g_client_fd >= 0) { close(g_client_fd); g_client_fd = -1; }
    pthread_mutex_unlock(&g_client_mutex);
    pthread_join(g_accept_thread, NULL);
}

int tcp_cmd_server_send_status(const status_report_t *report) {
    pthread_mutex_lock(&g_client_mutex);
    int fd = g_client_fd;
    pthread_mutex_unlock(&g_client_mutex);
    if (fd < 0) return -1;
    cmd_header_t hdr = { CMD_MAGIC, CMD_STATUS_REPORT, sizeof(status_report_t) };
    send(fd, &hdr, sizeof(hdr), MSG_NOSIGNAL);
    send(fd, report, sizeof(status_report_t), MSG_NOSIGNAL);
    return 0;
}
```

- [ ] **Step 3: Build and verify**

Run: `cd /mnt/A/yt/hust-tracker-3576 && make clean && make`

- [ ] **Step 4: Commit**

```
git add src/net/
git commit -m "feat: add TCP command server for remote control"
```

---

### Task A4: Integrate Servo Init/Deinit and TCP Server into main()

**Files:**
- Modify: `hust-tracker-3576/src/sample_huake.c`

- [ ] **Step 1: Add includes after line 671 (after `#include "sample_comm.h"`)**

```c
#include "servo_proto.h"
#include "servo_cmd.h"
#include "net/tcp_cmd_server.h"
```

- [ ] **Step 2: Add servo RX callback and TCP command handler before GetMediaBuffer0 (before line 765)**

Add `on_servo_rx()` callback (logs incoming servo messages).

Add `on_tcp_command()` handler that switches on cmd_id:
- `CMD_TRACK_XY`: sets `select_flag=1`, `Coordinate_X/Y`, `IS_TRACK=true`
- `CMD_TRACK_ID`: sets `select_flag=2`, `yolo_num_id`, `IS_TRACK=true`
- `CMD_TRACK_UNLOCK`: sets `IS_TRACK=false`
- `CMD_GIMBAL_CENTER`: calls `servo_gimbal_ctrl(SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_CENTER, SERVO_GIMBAL_FUNC_NONE), 128, 128)`
- `CMD_GIMBAL_DOWN90`: calls `servo_gimbal_ctrl(SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_DOWN_90, SERVO_GIMBAL_FUNC_NONE), 128, 128)`
- `CMD_GIMBAL_SET_ANGLE`: calls `servo_set_angle(pitch, yaw, 0)`
- `CMD_GIMBAL_SPEED`: calls `servo_gimbal_ctrl(NONE mode, yaw_speed, pitch_speed)`
- `CMD_GIMBAL_LOCK`: calls `servo_set_lock_mode(mode)`

Uses `extern` declarations for `select_flag`, `IS_TRACK`, `yolo_num_id`, `Coordinate_X`, `Coordinate_Y` from runtracker.cpp.

- [ ] **Step 3: Initialize servo and TCP server in main()**

After `rtsp_sync_video_ts(...)` (line 1028), add:
```c
if (servo_proto_init(NULL) != 0) {
    printf("servo_proto_init failed, continuing without servo\n");
} else {
    servo_proto_register_cb(on_servo_rx, NULL);
    servo_get_version();
    printf("Servo initialized OK\n");
}
if (tcp_cmd_server_start(on_tcp_command) != 0) {
    printf("TCP command server start failed\n");
}
```

- [ ] **Step 4: Add cleanup before exit (before `__FAILED:` label, line 1232)**

```c
tcp_cmd_server_stop();
servo_proto_deinit();
```

- [ ] **Step 5: Build and verify**

Run: `make clean && make`

- [ ] **Step 6: Commit**

```
git add src/sample_huake.c
git commit -m "feat: integrate servo init and TCP command server into main"
```

---

### Task A5: Add Vision Tracking Servo Control in processRGBData

**Files:**
- Modify: `hust-tracker-3576/src/tracker/runtracker.cpp`

- [ ] **Step 1: Add servo include at top (after line 25)**

```cpp
extern "C" {
#include "servo_cmd.h"
#include "net/cmd_protocol.h"
#include "net/tcp_cmd_server.h"
}
```

- [ ] **Step 2: Add servo tracking after drawBoxesByStatus() (after line 259)**

When `GLB_TrackerRes.nTrackerStatus == 3`:
- Compute target center: `cx = xmin + w/2`, `cy = ymin + h/2`
- Compute error from image center: `err_x = cx - width/2`, `err_y = cy - height/2`
- Call `servo_vision_track(err_x, err_y, 1, 0, w, h)`

Also send `status_report_t` via `tcp_cmd_server_send_status()` every frame.

- [ ] **Step 3: Stop servo tracking when track is lost**

In `ResetAllStatesOnTrackLost()`, inside `if (last_status == 3 && cur_status != 3)`, add:
```cpp
servo_vision_track(0, 0, 0, 0, 0, 0);
```

- [ ] **Step 4: Remove hardcoded test target (line 182)**

Change:
```cpp
sorted_box = getBoxById(yolo_num_id);
sorted_box = { 900, 500, 200, 100, 5 };
```
To:
```cpp
sorted_box = getBoxById(yolo_num_id);
```

- [ ] **Step 5: Build and verify**

Run: `make clean && make`

- [ ] **Step 6: Commit**

```
git add src/tracker/runtracker.cpp
git commit -m "feat: add servo vision tracking and TCP status reporting"
```

---

## Sub-Project B: Windows RTSP Streaming Client (Qt)

### Overview

Simplified Qt application based on SampleTool's smooth FFmpeg RTSP streaming. Replaces complex UI with focused controls for our use case.

```
+----------------------------------------------------------+
| HUST Tracker Client                              [_][X]  |
+------------------+---------------------------------------+
|  Connection      |                                       |
|  RTSP URL: [___] |          VIDEO DISPLAY                |
|  Device IP:[___] |       (click to select target)        |
|  [Connect][Stop] |                                       |
|                  |    Status: SEARCH | 5 targets         |
|  Target Select   |    Gimbal: yaw=12.5  pitch=-30.0     |
|  (o) Click       |                                       |
|  ( ) By ID: [__] |                                       |
|  [Lock] [Unlock] |                                       |
|                  |                                       |
|  Gimbal Control  |                                       |
|  [Center][Down90]|                                       |
|  Pitch: [__]     |                                       |
|  Yaw:   [__]     |                                       |
|  [Set Angle]     |                                       |
|  [Lock][Unlock]  |                                       |
|                  |                                       |
|  Log:            |                                       |
|  [............]  |                                       |
+------------------+---------------------------------------+
| Connected | TRACK | FPS: 30 | 2048 Kbps                 |
+----------------------------------------------------------+
```

### File Structure

```
HustTrackerClient/
+-- HustTrackerClient.pro          # Qt project file
+-- main.cpp                       # Entry point
+-- mainwindow.h / .cpp / .ui      # Main UI window
+-- videolabel.h / .cpp            # Custom QLabel: mouse click -> track coords
+-- streamworker.h / .cpp          # RTSP streaming thread (from SampleTool)
+-- cmdclient.h / .cpp             # TCP command client
+-- cmd_protocol.h                 # Shared protocol header (copy from device)
+-- Libs/                          # FFmpeg libraries (copy from SampleTool)
    +-- Libs.pri
    +-- include/
    +-- lib/
```

---

### Task B1: Create Project Structure and Copy FFmpeg Libs

**Files:**
- Create: `HustTrackerClient/` directory
- Copy: FFmpeg libs from SampleTool

- [ ] **Step 1: Create project directory and copy libs**

Create `/mnt/A/yt/HustTrackerClient/`.
Copy `/mnt/A/yt/_tmp_sampletool/SampleTool/Libs` into it.
Copy SampleTool icon as `app.ico`.

- [ ] **Step 2: Create HustTrackerClient.pro**

```qmake
QT += core gui widgets network
CONFIG += c++11
TARGET = HustTrackerClient
TEMPLATE = app

SOURCES += main.cpp mainwindow.cpp videolabel.cpp streamworker.cpp cmdclient.cpp
HEADERS += mainwindow.h videolabel.h streamworker.h cmdclient.h cmd_protocol.h
FORMS += mainwindow.ui

include(Libs/Libs.pri)
RC_ICONS = app.ico
```

- [ ] **Step 3: Create Libs.pri**

```qmake
INCLUDEPATH += $$PWD/include
win32 {
    LIBS += -L$$PWD/lib -lavformat -lavcodec -lavutil -lswscale
}
```

- [ ] **Step 4: Copy cmd_protocol.h**

Copy from device side `src/net/cmd_protocol.h` to `HustTrackerClient/cmd_protocol.h`.

- [ ] **Step 5: Commit**

---

### Task B2: Implement StreamWorker (RTSP Decode Thread)

**Files:**
- Create: `HustTrackerClient/streamworker.h`
- Create: `HustTrackerClient/streamworker.cpp`

Based on SampleTool StreamWorker but simplified: RTSP-only mode.

- [ ] **Step 1: Create streamworker.h**

QThread subclass with:
- `startStream(url, useTcp)`, `stopStream()`, `isStreaming()`
- Signal: `frameReady(QImage, codec, fps, bitrate)`
- Signal: `statusMessage(msg, level)`

- [ ] **Step 2: Create streamworker.cpp**

`run()` loops calling `openRtsp()` with 2s retry on disconnect.

`openRtsp()` pipeline (same as SampleTool):
1. `avformat_open_input` with rtsp_transport=tcp, stimeout=2000000
2. `avformat_find_stream_info`
3. Find video stream, get decoder
4. `avcodec_open2`
5. Create `sws_getContext` for YUV420P -> RGB24
6. Loop: `av_read_frame` -> `avcodec_send_packet` -> `avcodec_receive_frame` -> `sws_scale` -> emit QImage
7. Calculate bitrate from packet sizes per second window
8. Cleanup on disconnect

- [ ] **Step 3: Commit**

---

### Task B3: Implement TCP Command Client

**Files:**
- Create: `HustTrackerClient/cmdclient.h`
- Create: `HustTrackerClient/cmdclient.cpp`

- [ ] **Step 1: Create cmdclient.h**

QObject with QTcpSocket.
Methods for each command: `sendTrackXY`, `sendTrackID`, `sendTrackUnlock`, `sendGimbalCenter`, `sendGimbalDown90`, `sendGimbalSetAngle`, `sendGimbalSpeed`, `sendGimbalLock`.
Signal: `statusReceived(status_report_t)`.

- [ ] **Step 2: Create cmdclient.cpp**

- `sendCommand(cmd_id, payload, len)`: writes `cmd_header_t` + payload to socket.
- `onReadyRead()`: accumulates data in buffer, parses `cmd_header_t`, when `CMD_STATUS_REPORT` received, emits `statusReceived`.
- Each send method creates the appropriate payload struct and calls `sendCommand`.

- [ ] **Step 3: Commit**

---

### Task B4: Create VideoLabel (Clickable Video Widget)

**Files:**
- Create: `HustTrackerClient/videolabel.h`
- Create: `HustTrackerClient/videolabel.cpp`

- [ ] **Step 1: Create videolabel.h**

QLabel subclass with:
- `setVideoFrame(QPixmap)`, `setVideoSize(w, h)`
- Signal: `videoClicked(int x, int y)` in video coordinates
- Override: `mousePressEvent`, `paintEvent`

- [ ] **Step 2: Create videolabel.cpp**

`paintEvent`: draws pixmap letterboxed (KeepAspectRatio), stores `m_displayRect`.
`mousePressEvent`: maps label click position to video coordinates via `m_displayRect` ratio, emits `videoClicked`.

- [ ] **Step 3: Commit**

---

### Task B5: Create Main Window UI and Entry Point

**Files:**
- Create: `HustTrackerClient/mainwindow.h`
- Create: `HustTrackerClient/mainwindow.cpp`
- Create: `HustTrackerClient/mainwindow.ui`
- Create: `HustTrackerClient/main.cpp`

- [ ] **Step 1: Create mainwindow.ui**

Layout: QHBoxLayout with left panel (280px max, QVBoxLayout) and right panel (expanding).

Left panel groups:
- **Connection**: RTSP URL QLineEdit, Device IP QLineEdit, Connect/Disconnect buttons
- **Target Select**: QRadioButton "Click on Video" (default), QRadioButton "By ID" + QSpinBox, Lock/Unlock buttons
- **Gimbal Control**: Center/Down90 buttons, Pitch QSpinBox (-90..30), Yaw QSpinBox (-180..180), Set Angle button, Gimbal Lock/Unlock buttons
- **Log**: QTextEdit (read-only, max 150px height)

Right panel: VideoLabel (expanding) + status QLabel at bottom.

Custom widget: VideoLabel promoted from QLabel with header `videolabel.h`.

- [ ] **Step 2: Create mainwindow.h**

QMainWindow with StreamWorker, CmdClient members.
Slots for all button clicks and signal handlers.

- [ ] **Step 3: Create mainwindow.cpp**

Constructor: creates StreamWorker and CmdClient, connects all signals/slots.

Key slot implementations:
- `onConnectClicked`: starts stream and TCP connection
- `onDisconnectClicked`: stops both
- `onFrameReady`: converts QImage to QPixmap, sets on VideoLabel, updates statusbar
- `onVideoClicked`: if radio "Click" is checked, calls `m_cmd->sendTrackXY(x, y)`
- `onBtnLock`: if radio "ID" checked, calls `m_cmd->sendTrackID(id)`
- `onBtnUnlock`: calls `m_cmd->sendTrackUnlock()`
- Gimbal buttons: call corresponding CmdClient methods
- `onStatusReceived`: updates status label with tracker state, angles, target count
- `appendLog`: adds color-coded timestamped message to log QTextEdit

- [ ] **Step 4: Create main.cpp**

```cpp
#include <QApplication>
#include "mainwindow.h"
int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
```

- [ ] **Step 5: Commit**

---

### Task B6: Build and Test on Windows

- [ ] **Step 1: Open in Qt Creator, configure build kit (MSVC or MinGW)**
- [ ] **Step 2: Build and fix any platform-specific issues**
- [ ] **Step 3: Copy FFmpeg DLLs to output directory alongside .exe**
- [ ] **Step 4: Test with a test RTSP stream for video playback**
- [ ] **Step 5: Commit final adjustments**

---

## Integration Testing Checklist

1. Start device: `./hust_tracker_3576 -w 1920 -h 1080 -a /home/iqfiles/ ...`
2. Launch Windows client, enter RTSP URL and device IP, click Connect
3. Verify smooth video streaming
4. Click on a detected target -> verify tracker locks on (status changes to TRACK)
5. Verify gimbal follows target (err_x/err_y drives servo_vision_track)
6. Test Gimbal Center, Down 90, Set Angle
7. Test ID-based target selection
8. Test Unlock
9. Verify status bar updates with tracker state and gimbal angles
