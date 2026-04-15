
#ifndef __TRACKER_API_HPP__
#define __TRACKER_API_HPP__
// version: 1.2
// 撰写人：陈思佳
// 时间：20250708


#include "WF_PIXEL_FORMAT_E.hpp" // 需要包含的数据流buf格式的WF_PIXEL_FORMAT_E.hpp头文件

// 人为设置最大目标检测个数
#define MAX_TARGET_NUM 200

// 人为设置检测的类别
static const char *DetectClassNameEnum[] = {"person", "car", "drone"};

// 自动计算类别的数量
const int DetectClassNameEnumLen = sizeof(DetectClassNameEnum) / sizeof(DetectClassNameEnum[0]);

typedef enum
{
    STATUS_ID_WAIT = 1,     // 跟踪等待状态
    STATUS_ID_SEARCH = 2,   // 搜索状态
    STATUS_ID_TRACK = 3,    // 跟踪状态
    STATUS_ID_RECAP = 4,    // 预测重捕状态
    STATUS_ID_LOST = 5      // 跟丢状态
} Tracker_StatusID;

typedef enum
{
    CMD_ID_UNLOCK = 0,      // 跟踪解锁命令
    CMD_ID_LOCK = 1         // 跟踪锁定命令
} Tracker_LOCK_MOD;

typedef enum
{
    LOCK_MOD_MANUAL = 1,    // 手动模式
    LOCK_MOD_AUTO           // 自动模式
} Tracker_LockMod;

typedef struct
{
    unsigned short nLockMod;    // 跟踪模式解锁键
    unsigned short nTargetId;   // 目标ID
    unsigned short nCmd;        // 命令
    unsigned short nLock_x;     // 锁定目标的起始X
    unsigned short nLock_y;     // 锁定目标的起始Y
    unsigned short nLock_w;     // 锁定目标的宽w
    unsigned short nLock_h;     // 锁定目标的高h
    unsigned int nFrameId;      // 帧编号
} Tracker_Cmd;

typedef struct
{
    float xmin;     // 检测框左上角x坐标
    float ymin;     // 检测框左上角y坐标
    float w;        // 检测框宽w
    float h;        // 检测框高h
    float score;    // 检测框置信度score
    int classId;    // 检测框类别classId
    int frame_id;   // 检测框的帧编号
    int track_id;   // 检测框的轨迹编号track_id
    int track_len;  // 检测框的轨迹长度track_len
    bool inArea;    // 检测框是否在指定区域inArea。暂时是没有使用的
} Object_Property;


typedef struct
{
    int frame_id;               // 帧编号
    int nArithTime;             // 算法耗时
    /*      跟踪相关状态    */
    int nTrackerStatus;         // 跟踪器状态
    float xmin;                 // 单目标起始X
    float ymin;                 // 单目标起始y
    float w;                    // 单目标宽w
    float h;                    // 单目标高h
    /*      检测相关状态    */
    int ntargetnum;             // 多目标个数
    Object_Property *Objects;   // 多目标列表
} Tracker_Objects;


typedef void TrackerResultCallBack(Tracker_Objects *userData); // 结果回调函数
unsigned long arith_get_time_ms(); // 获取当前时间戳，单位为毫秒


// ======================== 可调用的函数 ========================
bool Tracker_Register(); // TTTtracker 注册及内存分配
bool Tracker_SetCmd(Tracker_Cmd &cmd); // 设置跟踪器命令：初始化、发送锁定跟踪目标、解锁跟踪目标等命令
bool Tracker_UpdateStatus(unsigned char *buf, int nWidth, int nHeight, WF_PIXEL_FORMAT_E enumPixelType, TrackerResultCallBack callback); // 跟踪器运行函数

// // ============ 需要 opencv ============
// // 如果不想使用 OpenCV，可以注释掉下面的函数定义即可
#include <opencv2/opencv.hpp>
bool Tracker_CVMat_UpdateStatus(cv::Mat &im, int nWidth, int nHeight, TrackerResultCallBack callback); // 跟踪器运行函数
bool Tracker_StatusDraw(cv::Mat &im, int nWidth, int nHeight);

#endif
