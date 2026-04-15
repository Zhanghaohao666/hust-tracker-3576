/*
 * servo_test.c  —  伺服云台基础功能测试
 *
 * 测试序列:
 *   1. 初始化伺服协议栈
 *   2. 查询固件版本
 *   3. 归中
 *   4. 等待 3 秒
 *   5. 下视 90 度
 *   6. 等待 3 秒
 *   7. 归中
 *   8. 等待 2 秒
 *   9. 指定角度 (pitch=-45, yaw=90)
 *  10. 等待 3 秒
 *  11. 归中
 *  12. 清理退出
 *
 * 编译: 见同目录 Makefile，或:
 *   aarch64-none-linux-gnu-gcc -o servo_test servo_test.c \
 *     -I../RK3576_ThirdLibs/include/libservo \
 *     -L../RK3576_ThirdLibs/lib \
 *     -lservo -llog -luart -lcommcore -lpthread
 *
 * 运行 (在 RK3576 设备上):
 *   export LD_LIBRARY_PATH=/path/to/libs:$LD_LIBRARY_PATH
 *   ./servo_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "servo_cmd.h"
#include "servo_proto.h"

static volatile int g_quit = 0;

static void sig_handler(int sig)
{
    (void)sig;
    g_quit = 1;
}

/* 接收回调：打印伺服返回的消息 */
static void on_servo_rx(uint16_t msgid,
                        uint8_t  frame_type,
                        uint8_t *payload,
                        uint8_t  len,
                        void    *user)
{
    (void)user;
    printf("[RX] msgid=0x%04X frame_type=%u len=%u\n", msgid, frame_type, len);

    if (msgid == SERVO_MSG_STATUS_REPORT && len >= sizeof(servo_status_report_t)) {
        servo_status_report_t *s = (servo_status_report_t *)payload;
        printf("     gimbal_status=%u motor=%u follow=%u lock=%u imu_temp=%.1f\n",
               s->gimbal_status, s->motor_state, s->follow_state,
               s->lock_state, s->imu_temp / 10.0);
    }

    if (msgid == SERVO_MSG_ATTITUDE_REPORT && len >= sizeof(servo_attitude_report_t)) {
        servo_attitude_report_t *a = (servo_attitude_report_t *)payload;
        printf("     yaw=%.2f pitch=%.2f roll=%.2f\n",
               a->yaw_angle / 100.0,
               a->pitch_angle / 100.0,
               a->roll_angle / 100.0);
    }
}

static void wait_sec(int sec)
{
    for (int i = 0; i < sec && !g_quit; i++) {
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    const char *dev = NULL; /* NULL = 使用 libservo 默认串口 (/dev/ttyS10) */
    if (argc > 1) {
        dev = argv[1];
        printf("[INFO] 使用指定串口: %s\n", dev);
    }

    /* ---- 1. 初始化 ---- */
    printf("=== 伺服协议初始化 ===\n");
    if (servo_proto_init(dev) != 0) {
        fprintf(stderr, "[ERROR] servo_proto_init 失败！检查串口设备是否存在。\n");
        return 1;
    }
    printf("[OK] 伺服协议初始化成功\n");

    servo_proto_register_cb(on_servo_rx, NULL);

    /* ---- 2. 获取版本 ---- */
    printf("\n=== 获取固件版本 ===\n");
    servo_get_version();
    wait_sec(1);

    /* ---- 3. 归中 ---- */
    printf("\n=== [测试 1] 云台归中 ===\n");
    int ret = servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_CENTER, SERVO_GIMBAL_FUNC_NONE),
        128, 128);
    printf("servo_gimbal_ctrl(CENTER) 返回: %d\n", ret);
    printf("等待 3 秒...\n");
    wait_sec(3);

    if (g_quit) goto cleanup;

    /* ---- 4. 下视 90 度 ---- */
    printf("\n=== [测试 2] 云台下视 90 度 ===\n");
    ret = servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_DOWN_90, SERVO_GIMBAL_FUNC_NONE),
        128, 128);
    printf("servo_gimbal_ctrl(DOWN_90) 返回: %d\n", ret);
    printf("等待 3 秒...\n");
    wait_sec(3);

    if (g_quit) goto cleanup;

    /* ---- 5. 再次归中 ---- */
    printf("\n=== [测试 3] 云台归中 ===\n");
    ret = servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_CENTER, SERVO_GIMBAL_FUNC_NONE),
        128, 128);
    printf("servo_gimbal_ctrl(CENTER) 返回: %d\n", ret);
    printf("等待 2 秒...\n");
    wait_sec(2);

    if (g_quit) goto cleanup;

    /* ---- 6. 指定角度: pitch=-45, yaw=90 ---- */
    printf("\n=== [测试 4] 指定角度: pitch=-45, yaw=90 ===\n");
    ret = servo_set_angle(-45, 90, SERVO_SET_ANGLE_MODE_ATTITUDE);
    printf("servo_set_angle(pitch=-45, yaw=90) 返回: %d\n", ret);
    printf("等待 3 秒...\n");
    wait_sec(3);

    if (g_quit) goto cleanup;

    /* ---- 7. 最终归中 ---- */
    printf("\n=== [测试 5] 最终归中 ===\n");
    ret = servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_CENTER, SERVO_GIMBAL_FUNC_NONE),
        128, 128);
    printf("servo_gimbal_ctrl(CENTER) 返回: %d\n", ret);
    wait_sec(2);

cleanup:
    printf("\n=== 清理退出 ===\n");
    servo_proto_deinit();
    printf("[OK] 测试完成\n");
    return 0;
}
