/* =============================================================================
 * servo_demo.c —— 伺服云台控制独立测试程序
 * -----------------------------------------------------------------------------
 * 用法:
 *   ./servo_demo center             归中（yaw+pitch 全部回中）
 *   ./servo_demo angle <p> <y>      指定角度控制 (pitch, yaw)
 *   ./servo_demo speed <ex> <ey>    用 gimbal_ctrl 速度控制模拟跟踪 (单次 2s)
 *   ./servo_demo speed_loop         持续速度控制 err_x=200 (Ctrl+C退出)
 *
 * 交叉编译:
 *   cd /mnt/A/yt/hust-tracker-3576
 *   make -f tools/Makefile.servo_demo
 *
 * 部署到板子后运行即可，无需摄像头 / RTSP / 跟踪库。
 * ============================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "servo_proto.h"
#include "servo_cmd.h"

static volatile int g_quit = 0;

static void sig_handler(int sig) {
    (void)sig;
    g_quit = 1;
}

/* ---- 伺服接收回调：打印姿态反馈 ---- */
static void on_servo_rx(uint16_t msgid, uint8_t frame_type,
                        uint8_t *payload, uint8_t len, void *user) {
    (void)user;
    (void)frame_type;

    if (msgid == 0x0002) {
        servo_attitude_report_t *att = (servo_attitude_report_t *)payload;

        static int cnt = 0;
        if ((++cnt) % 50 == 0) {  /* ~0.5s @ 100Hz */
            printf("[ATT] yaw=%.2f° pitch=%.2f° roll=%.2f° | "
                   "spd(y=%.2f p=%.2f) | "
                   "frame(y=%.2f p=%.2f) | "
                   "trk_off(%d, %d)\n",
                   att->yaw_angle   * 0.01f,
                   att->pitch_angle * 0.01f,
                   att->roll_angle  * 0.01f,
                   att->yaw_speed   * 0.01f,
                   att->pitch_speed * 0.01f,
                   att->yaw_frame_angle   * 0.01f,
                   att->pitch_frame_angle * 0.01f,
                   att->track_offset_x,
                   att->track_offset_y);
        }
    }
}

/* ---- 初始化伺服 ---- */
static int servo_init(void) {
    if (servo_proto_init(NULL) != 0) {
        fprintf(stderr, "ERROR: servo_proto_init failed!\n");
        return -1;
    }
    servo_proto_register_cb(on_servo_rx, NULL);
    printf("[INIT] servo_proto_init OK\n");

    servo_get_version();
    usleep(200000);

    servo_set_lock_mode(SERVO_LOCK_MODE_EXIT);
    usleep(50000);

    servo_set_max_wheel_speed(100, 100);
    usleep(50000);

    printf("[INIT] lock=EXIT, wheel_speed=100\n");
    return 0;
}

/* ================================================================
 * 像素偏差 → 速度值映射
 *
 * servo_gimbal_ctrl 的 yaw/pitch 参数:
 *   128 = 停止
 *   0   = 最大反向速度
 *   255 = 最大正向速度
 *
 * 映射逻辑:
 *   speed = 128 + clamp(err * gain, -127, +127)
 *   死区: |err| < DEADZONE 时 speed = 128
 * ================================================================ */
#define DEADZONE     15     /* 像素死区 */
#define ERR_SATURATE 400.0f /* 像素误差饱和值 (此值对应最大速度) */
#define MAX_SPEED    80     /* 最大速度偏移量 (0~127, 越大越快) */

static uint8_t err_to_speed(int16_t err) {
    if (err > -DEADZONE && err < DEADZONE)
        return 128;

    float fval = (float)err / ERR_SATURATE * (float)MAX_SPEED;
    int ival = (int)(fval);
    if (ival >  MAX_SPEED) ival =  MAX_SPEED;
    if (ival < -MAX_SPEED) ival = -MAX_SPEED;
    return (uint8_t)(128 + ival);
}

/* ================================================================
 * 测试 1: 归中
 * ================================================================ */
static void test_center(void) {
    printf("\n===== TEST: GIMBAL CENTER =====\n");

    servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_CENTER, SERVO_GIMBAL_FUNC_NONE),
        128, 128);

    printf("已发送归中指令。等待 3 秒...\n");
    sleep(3);
    printf("归中测试完成。\n");
}

/* ================================================================
 * 测试 2: 指定角度
 * ================================================================ */
static void test_angle(int16_t pitch, int16_t yaw) {
    printf("\n===== TEST: SET ANGLE (pitch=%d° yaw=%d°) =====\n", pitch, yaw);
    servo_set_angle(pitch, yaw, 0);
    printf("已发送。等待 3 秒...\n");
    sleep(3);
    printf("角度测试完成。\n");
}

/* ================================================================
 * 测试 3: 速度控制跟踪 —— 单次
 *
 *   用 servo_gimbal_ctrl 把像素偏差映射为转速
 *   持续 3 秒后停止
 * ================================================================ */
static void test_speed_track(int16_t err_x, int16_t err_y) {
    uint8_t yaw_spd   = err_to_speed(err_x);
    uint8_t pitch_spd = err_to_speed(err_y);

    printf("\n===== TEST: SPEED TRACK =====\n");
    printf("err_x=%d → yaw_speed=%u (128=stop)\n", err_x, yaw_spd);
    printf("err_y=%d → pitch_speed=%u (128=stop)\n", err_y, pitch_spd);

    printf("持续发送 3 秒 (~30fps)...\n");
    for (int i = 0; i < 90 && !g_quit; i++) {
        servo_gimbal_ctrl(
            SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_NONE, SERVO_GIMBAL_FUNC_NONE),
            yaw_spd, pitch_spd);
        usleep(33333);
    }

    /* 停止 */
    servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_NONE, SERVO_GIMBAL_FUNC_NONE),
        128, 128);

    printf("速度跟踪测试完成。\n");
}

/* ================================================================
 * 测试 4: 持续速度控制跟踪 (Ctrl+C 退出)
 * ================================================================ */
static void test_speed_loop(void) {
    const int16_t err_x = 200;
    const int16_t err_y = 0;
    uint8_t yaw_spd   = err_to_speed(err_x);
    uint8_t pitch_spd = err_to_speed(err_y);

    printf("\n===== TEST: SPEED TRACK LOOP =====\n");
    printf("err_x=%d → yaw_speed=%u, Ctrl+C 退出\n", err_x, yaw_spd);

    while (!g_quit) {
        servo_gimbal_ctrl(
            SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_NONE, SERVO_GIMBAL_FUNC_NONE),
            yaw_spd, pitch_spd);
        usleep(33333);
    }

    /* 停止 */
    servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_NONE, SERVO_GIMBAL_FUNC_NONE),
        128, 128);

    printf("\n持续跟踪测试结束。\n");
}

/* ================================================================
 * 测试 5: servo_vision_track —— 不切模式，直接发视觉偏差
 * ================================================================ */
static void test_vision_track_raw(int16_t err_x, int16_t err_y) {
    printf("\n===== TEST: VISION_TRACK RAW (不切TRACK模式) =====\n");
    printf("err_x=%d, err_y=%d, enable=1, box=100x100\n", err_x, err_y);
    printf("持续发送 5 秒 (~30fps), 观察云台是否运动...\n");
    printf("参数: en=2(跟踪态), flag=1(跟踪成功)\n");

    for (int i = 0; i < 150 && !g_quit; i++) {
        servo_vision_track(err_x, err_y, 2, 1, 100, 100);
        usleep(33333);
    }
    /* 发送解锁停止 */
    servo_vision_track(0, 0, 0, 0, 0, 0);
    printf("完成。\n");
}

/* ================================================================
 * 测试 6: servo_vision_track —— 先切到 FUNC_TRACK 模式再发偏差
 * ================================================================ */
static void test_vision_track_with_mode(int16_t err_x, int16_t err_y) {
    printf("\n===== TEST: VISION_TRACK + FUNC_TRACK MODE =====\n");

    /* 先切换到跟踪功能模式 */
    printf("Step 1: 切换到 FUNC_TRACK 模式...\n");
    servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_NONE, SERVO_GIMBAL_FUNC_TRACK),
        128, 128);
    usleep(500000);  /* 等 500ms 让伺服切换模式 */

    printf("Step 2: 发送 vision_track(err_x=%d, err_y=%d, en=2, flag=1) 持续 5 秒...\n", err_x, err_y);
    for (int i = 0; i < 150 && !g_quit; i++) {
        servo_vision_track(err_x, err_y, 2, 1, 100, 100);
        usleep(33333);
    }

    /* 停止 */
    servo_vision_track(0, 0, 0, 0, 0, 0);
    usleep(100000);

    /* 切回普通模式 */
    printf("Step 3: 切回 FUNC_NONE 模式\n");
    servo_gimbal_ctrl(
        SERVO_GIMBAL_MODE_MAKE(SERVO_GIMBAL_WORK_NONE, SERVO_GIMBAL_FUNC_NONE),
        128, 128);

    printf("完成。\n");
}

/* ================================================================
 * 测试 7: vision_track 多位置依次测试
 *   归中 → 偏右(200,0) → 偏左(-200,0) → 偏下(0,200) → 偏上(0,-200) → 归中
 *   每组尝试两种方式: 不切模式 / 切TRACK模式
 * ================================================================ */
static void test_vision_track_all(void) {
    printf("\n====== VISION_TRACK 完整测试 ======\n\n");

    struct { int16_t ex, ey; const char *desc; } cases[] = {
        { 200,    0, "偏右 (期望云台往右转)" },
        {-200,    0, "偏左 (期望云台往左转)" },
        {   0,  200, "偏下 (期望云台往下转)" },
        {   0, -200, "偏上 (期望云台往上转)" },
    };
    int ncases = sizeof(cases) / sizeof(cases[0]);

    /* --- 方式 A: 不切模式直接发 --- */
    printf("======== 方式A: 不切模式, 直接发 servo_vision_track ========\n");
    test_center();
    sleep(2);

    for (int i = 0; i < ncases && !g_quit; i++) {
        printf("\n--- A-%d: %s (err_x=%d, err_y=%d) ---\n",
               i + 1, cases[i].desc, cases[i].ex, cases[i].ey);
        test_vision_track_raw(cases[i].ex, cases[i].ey);
        sleep(2);
        test_center();
        sleep(2);
    }

    /* --- 方式 B: 先切 FUNC_TRACK 再发 --- */
    printf("\n======== 方式B: 先切 FUNC_TRACK 模式, 再发 servo_vision_track ========\n");
    test_center();
    sleep(2);

    for (int i = 0; i < ncases && !g_quit; i++) {
        printf("\n--- B-%d: %s (err_x=%d, err_y=%d) ---\n",
               i + 1, cases[i].desc, cases[i].ex, cases[i].ey);
        test_vision_track_with_mode(cases[i].ex, cases[i].ey);
        sleep(2);
        test_center();
        sleep(2);
    }

    printf("\n====== VISION_TRACK 完整测试结束 ======\n");
    printf("请检查:\n");
    printf("  1. 方式A/B 哪种云台有运动?\n");
    printf("  2. 运动方向是否正确?\n");
    printf("  3. [ATT] 日志中 trk_off(x,y) 有无变化?\n");
}

/* ================================================================ */
static void print_usage(const char *prog) {
    printf("伺服云台测试工具\n\n");
    printf("用法:\n");
    printf("  %s center               归中 (yaw+pitch)\n", prog);
    printf("  %s angle <pitch> <yaw>  指定角度 (度)\n", prog);
    printf("  %s speed <err_x> <err_y>  速度控制跟踪 (单次3秒)\n", prog);
    printf("  %s speed_loop           持续速度跟踪 err_x=200 (Ctrl+C)\n", prog);
    printf("  %s vtrack <err_x> <err_y>  vision_track不切模式 (5秒)\n", prog);
    printf("  %s vtrack_mode <ex> <ey>   vision_track+FUNC_TRACK (5秒)\n", prog);
    printf("  %s vtrack_all              vision_track多位置完整测试\n", prog);
    printf("  %s all                  依次运行全部速度控制测试\n", prog);
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    if (servo_init() != 0) {
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "center") == 0) {
        test_center();

    } else if (strcmp(cmd, "angle") == 0) {
        int16_t p = (argc >= 3) ? (int16_t)atoi(argv[2]) : 0;
        int16_t y = (argc >= 4) ? (int16_t)atoi(argv[3]) : 0;
        test_angle(p, y);

    } else if (strcmp(cmd, "speed") == 0) {
        int16_t ex = (argc >= 3) ? (int16_t)atoi(argv[2]) : 200;
        int16_t ey = (argc >= 4) ? (int16_t)atoi(argv[3]) : 0;
        test_speed_track(ex, ey);

    } else if (strcmp(cmd, "speed_loop") == 0) {
        test_speed_loop();

    } else if (strcmp(cmd, "vtrack") == 0) {
        int16_t ex = (argc >= 3) ? (int16_t)atoi(argv[2]) : 200;
        int16_t ey = (argc >= 4) ? (int16_t)atoi(argv[3]) : 0;
        test_vision_track_raw(ex, ey);

    } else if (strcmp(cmd, "vtrack_mode") == 0) {
        int16_t ex = (argc >= 3) ? (int16_t)atoi(argv[2]) : 200;
        int16_t ey = (argc >= 4) ? (int16_t)atoi(argv[3]) : 0;
        test_vision_track_with_mode(ex, ey);

    } else if (strcmp(cmd, "vtrack_all") == 0) {
        test_vision_track_all();

    } else if (strcmp(cmd, "all") == 0) {
        printf("====== 全部测试开始 ======\n\n");

        /* 1) 归中 */
        test_center();
        sleep(1);

        /* 2) 速度控制: err_x=200 (应该往右转) */
        printf("--- 测试: err_x=+200 (期望往右转) ---\n");
        test_speed_track(200, 0);
        sleep(2);

        /* 3) 归中恢复 */
        test_center();
        sleep(1);

        /* 4) 速度控制: err_x=-200 (应该往左转) */
        printf("--- 测试: err_x=-200 (期望往左转) ---\n");
        test_speed_track(-200, 0);
        sleep(2);

        /* 5) 归中恢复 */
        test_center();
        sleep(1);

        /* 6) 速度控制: err_y=200 (应该往下转) */
        printf("--- 测试: err_y=+200 (期望往下转) ---\n");
        test_speed_track(0, 200);
        sleep(2);

        /* 7) 归中恢复 */
        test_center();
        sleep(1);

        /* 8) 速度控制: err_y=-200 (应该往上转) */
        printf("--- 测试: err_y=-200 (期望往上转) ---\n");
        test_speed_track(0, -200);
        sleep(2);

        /* 9) 最终归中 */
        test_center();

        printf("\n====== 全部测试结束 ======\n");

    } else {
        fprintf(stderr, "未知命令: %s\n", cmd);
        print_usage(argv[0]);
        servo_proto_deinit();
        return 1;
    }

    printf("\n清理中...\n");
    servo_proto_deinit();
    printf("Done.\n");
    return 0;
}
