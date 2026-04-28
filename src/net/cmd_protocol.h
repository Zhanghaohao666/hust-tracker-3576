#ifndef CMD_PROTOCOL_H
#define CMD_PROTOCOL_H

#include <stdint.h>

#define CMD_MAGIC           0xAA
#define CMD_PORT            9000

/* Client -> Device command IDs */
#define CMD_TRACK_XY        0x01
#define CMD_TRACK_ID        0x02
#define CMD_TRACK_UNLOCK    0x03
#define CMD_GIMBAL_CENTER   0x10
#define CMD_GIMBAL_DOWN90   0x11
#define CMD_GIMBAL_SET_ANGLE 0x12
#define CMD_GIMBAL_SPEED    0x13
#define CMD_GIMBAL_LOCK     0x14

/* Device -> Client */
#define CMD_STATUS_REPORT   0x80
#define CMD_TARGET_LIST     0x81

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
    uint8_t yaw_speed;   /* 0-255, 128 = stop */
    uint8_t pitch_speed;
} cmd_gimbal_speed_t;

typedef struct {
    uint8_t mode; /* 0=unlock, 1=lock */
} cmd_gimbal_lock_t;

typedef struct {
    uint8_t  tracker_status;  /* 1=WAIT,2=SEARCH,3=TRACK,4=RECAP,5=LOST */
    int16_t  track_x;
    int16_t  track_y;
    int16_t  track_w;
    int16_t  track_h;
    int16_t  yaw_angle;       /* 0.01 deg units */
    int16_t  pitch_angle;     /* 0.01 deg units */
    uint8_t  target_count;
} status_report_t;

/* Single target info for CMD_TARGET_LIST */
typedef struct {
    int16_t  x;
    int16_t  y;
    int16_t  w;
    int16_t  h;
    int32_t  id;
} target_info_t;   /* 12 bytes */

#define MAX_TARGETS_PER_LIST  32

#pragma pack(pop)

#endif /* CMD_PROTOCOL_H */
