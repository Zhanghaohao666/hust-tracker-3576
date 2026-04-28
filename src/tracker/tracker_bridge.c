/* =============================================================================
 * tracker_bridge.c —— 跟踪结果到 伺服 + TCP 上报 的桥接层实现
 * -----------------------------------------------------------------------------
 * 该文件把所有 servo / TCP 细节集中在这里，避免 runtracker.cpp 直接依赖。
 * 如需修改跟踪结果上报逻辑，只改本文件即可，跟踪算法本身不动。
 * ============================================================================= */

#include "tracker_bridge.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "servo_cmd.h"
#include "net/cmd_protocol.h"
#include "net/tcp_cmd_server.h"

/* 伺服初始化是否成功，由 sample_huake.c 维护 */
extern int g_servo_ok;

static int s_servo_tracking = 0;

void tracker_bridge_report(int tracker_status,
                           int xmin, int ymin, int w, int h,
                           int img_w, int img_h,
                           int target_count)
{
    /* ---- 1) 跟踪态：用伺服原生视觉跟踪接口 ---- */
    if (tracker_status == 3) {
        int16_t err_x = -(int16_t)((float)xmin + (float)w * 0.5f - (float)img_w * 0.5f);
        int16_t err_y = -(int16_t)((float)ymin + (float)h * 0.5f - (float)img_h * 0.5f);

        if (g_servo_ok) {
            /* en=2: 跟踪状态, flag=1: 跟踪成功 */
            servo_vision_track(err_x, err_y, 2, 1,
                               (uint16_t)w, (uint16_t)h);
        }
        s_servo_tracking = 1;
    } else {
        /* 退出跟踪态: en=0(解锁), flag=0 */
        if (s_servo_tracking && g_servo_ok) {
            servo_vision_track(0, 0, 0, 0, 0, 0);
            s_servo_tracking = 0;
        }
    }

    /* ---- 2) 把当前状态打包成报文，发给客户端 ---- */
    status_report_t rpt;
    rpt.tracker_status = (uint8_t)tracker_status;
    rpt.track_x        = (int16_t)xmin;
    rpt.track_y        = (int16_t)ymin;
    rpt.track_w        = (int16_t)w;
    rpt.track_h        = (int16_t)h;
    rpt.yaw_angle      = 0;        /* TODO: 后续接入伺服姿态反馈 */
    rpt.pitch_angle    = 0;        /* TODO: 后续接入伺服姿态反馈 */
    rpt.target_count   = (uint8_t)target_count;
    tcp_cmd_server_send_status(&rpt);
}

void tracker_bridge_on_track_lost(void)
{
    if (g_servo_ok && s_servo_tracking) {
        servo_vision_track(0, 0, 0, 0, 0, 0);
        s_servo_tracking = 0;
    }
}

void tracker_bridge_send_targets(const void *objects, int count)
{
    if (count <= 0) {
        uint8_t zero = 0;
        tcp_cmd_server_send_target_list(NULL, zero);
        return;
    }
    if (count > MAX_TARGETS_PER_LIST) count = MAX_TARGETS_PER_LIST;

    /* Object_Property: float xmin,ymin,w,h,score; int classId,frame_id,track_id,track_len; bool inArea */
    typedef struct {
        float xmin, ymin, w, h, score;
        int classId, frame_id, track_id, track_len;
        int inArea;  /* bool padded to int in C */
    } ObjProp;
    const ObjProp *objs = (const ObjProp *)objects;

    target_info_t list[MAX_TARGETS_PER_LIST];
    for (int i = 0; i < count; i++) {
        list[i].x  = (int16_t)objs[i].xmin;
        list[i].y  = (int16_t)objs[i].ymin;
        list[i].w  = (int16_t)objs[i].w;
        list[i].h  = (int16_t)objs[i].h;
        list[i].id = (int32_t)objs[i].track_id;
    }
    tcp_cmd_server_send_target_list(list, (uint8_t)count);
}
