#ifndef TRACKER_BRIDGE_H
#define TRACKER_BRIDGE_H

/* =============================================================================
 * tracker_bridge —— 跟踪结果到 伺服 + TCP 上报 的桥接层
 * -----------------------------------------------------------------------------
 * 目的：让 runtracker.cpp 专注于跟踪算法本身，不直接依赖 servo / TCP 模块。
 *      runtracker.cpp 只需 include 本头文件，并在合适位置调用 2 个函数。
 *
 * 实现见：tracker_bridge.c
 * 内部依赖：servo_cmd.h、net/tcp_cmd_server.h、net/cmd_protocol.h、g_servo_ok
 * ============================================================================= */

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
 * 每帧调用一次：根据当前跟踪结果驱动伺服并向客户端上报状态。
 *
 * 参数：
 *   tracker_status —— 跟踪状态（1=WAIT, 2=SEARCH, 3=TRACK, 4=RECAP, 5=LOST）
 *   xmin, ymin     —— 跟踪框左上角（仅在 status==3 时有意义）
 *   w, h           —— 跟踪框宽高（仅在 status==3 时有意义）
 *   img_w, img_h   —— 当前图像宽高（用于计算图像中心，转换为相对偏差）
 *   target_count   —— YOLO 检测到的目标数（写入状态报文）
 *
 * 行为：
 *   - 当 status==3 时：根据跟踪框中心与图像中心的偏差驱动伺服跟随。
 *   - 始终：把当前状态打包成 status_report_t 通过 TCP 发给客户端。
 * ----------------------------------------------------------------------------- */
void tracker_bridge_report(int tracker_status,
                           int xmin, int ymin, int w, int h,
                           int img_w, int img_h,
                           int target_count);

/* -----------------------------------------------------------------------------
 * 跟踪丢失时调用一次：让伺服停止跟随。
 * 通常在跟踪状态从 3 跳出时（例如 ResetAllStatesOnTrackLost 中）调用。
 * ----------------------------------------------------------------------------- */
void tracker_bridge_on_track_lost(void);

/* -----------------------------------------------------------------------------
 * 每帧调用：把 YOLO 检测到的目标列表发给客户端。
 * targets: 目标数组（x,y,w,h,id），count: 目标数量。
 * ----------------------------------------------------------------------------- */
void tracker_bridge_send_targets(const void *targets, int count);

#ifdef __cplusplus
}
#endif

#endif /* TRACKER_BRIDGE_H */
