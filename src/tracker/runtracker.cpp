#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <chrono>
#include <stdio.h>
#include <string.h>
#include <rga/RgaApi.h>
using namespace std;
using namespace cv;
#include <opencv2/opencv.hpp>
#include <algorithm>
#include "runtracker.hpp"
#include <vector>
#include <sstream>
#include <string>
#include <ctime>
#include "../include/Tracker_api.hpp"
#include "../include/dynamic.hpp"
#include "draw_id.hpp"
#include <opencv2/imgproc.hpp>
#include <array>
#include <fstream>  
#include <sys/stat.h> 
#include <chrono>
#include <thread>
#include <pthread.h>

// =============================================================================
// 【外部桥接】跟踪结果上报到 伺服 + TCP 客户端
// 本文件只负责跟踪算法。所有 servo / TCP 细节封装在 tracker_bridge.{h,c}。
// 仅通过下面 2 个简单函数与外界交互：
//   - tracker_bridge_report()      每帧上报跟踪结果
//   - tracker_bridge_on_track_lost() 跟踪丢失时通知
// =============================================================================
extern "C" {
#include "tracker_bridge.h"
}

/******************************/
// std::ostringstream oss_targets;


//*******opt_flow_args******/

// YoloBox_norm : Normalized coordinates (0-1)
static std::vector<YoloBox_norm> bbox_norm_last;
static std::vector<YoloBox_norm> bbox_norm_now;
static std::vector<YoloBox_norm> out_boxes;
static cv::Mat photo_last;

//*******SELECT_CONTROL_FLAG******/
int select_flag = 2; // 0: By Velocity; 1: By XY; 2: By ID
bool IS_TRACK = false;   // 默认不跟踪，等待客户端下发指令
int yolo_num_id  = 0;
int Coordinate_X = 1000;
int Coordinate_Y = 500;
pthread_mutex_t g_select_mutex = PTHREAD_MUTEX_INITIALIZER;

// 客户端 UNLOCK 请求标志位：TCP 线程置位，跟踪线程下一帧消费并强制解锁 KCF
// 必须用此机制：因为跟踪态(status==3)时 IS_TRACK 标志根本不会被检查，
// 单独 set IS_TRACK=false 无法停止已经在跑的 KCF 跟踪。
bool g_unlock_requested = false;

//*******Yolo_template_args******/
// YoloBox :  coordinates (pixel)
static std::vector<YoloBox> g_prev;


/******tracking control********/
// static int find_boxes_buffer = 0; 
bool is_last_frame_ok = false; //todo
bool TRACK = false;


/******globals********/
Object_Property GLB_Objects[MAX_TARGET_NUM];
Tracker_Objects GLB_TrackerRes;
int nFrmCnt = 0;
int ntargetnum = 0;

bool is_velocity_ok = false;//todo
int track_frame_cnt = 50;
int velocity_track_frame_cnt = 200;



void tResCallBack(Tracker_Objects *userData)
{
    memcpy(&GLB_TrackerRes, userData, sizeof(Tracker_Objects));
    ntargetnum = GLB_TrackerRes.ntargetnum;
    if (ntargetnum < 0) ntargetnum = 0;
    if (ntargetnum > MAX_TARGET_NUM) ntargetnum = MAX_TARGET_NUM;
    if (ntargetnum > 0 && userData->Objects != NULL) {
        memcpy(&GLB_Objects, &userData->Objects[0], ntargetnum * sizeof(Object_Property));
    }
}


// V1 20260318 11:21
void processRGBData(unsigned char* pData, int width, int height){

    using namespace std::chrono;
    // first_time_tag
    auto t0 = std::chrono::high_resolution_clock::now();
    cv::Mat inputMat(height, width, CV_8UC3, pData);

    // 【内存优化】使用静态 Mat 避免每帧重复分配内存
    // VPSS 输出 1920x1088 (ISP 16字节对齐), 跟踪库需要 1920x1080
    static cv::Mat b;
    if (b.empty() || b.cols != 1920 || b.rows != 1080) {
        b.create(1080, 1920, CV_8UC3);
    }
    cv::resize(inputMat, b, cv::Size(1920, 1080), 0, 0, cv::INTER_LINEAR);
    width = 1920;
    height = 1080;


    // start initializing / detecting / tracking
    if (0 == nFrmCnt)
    {  
        printf("Tracker_Register init.\n");
        Tracker_Register(); // 跟踪器 注册及内存分配
        printf("Tracker_Register is ok.\n");
    }

    if (1 == nFrmCnt)
    {
        Tracker_Cmd nTrackerCMD;
        memset(&nTrackerCMD, 0, sizeof(nTrackerCMD));
        nTrackerCMD.nCmd = CMD_ID_UNLOCK; // 跟踪解锁
        nTrackerCMD.nLockMod = LOCK_MOD_MANUAL; // 手动锁定
        nTrackerCMD.nFrameId = nFrmCnt;
        Tracker_SetCmd(nTrackerCMD);  
        printf("Tracker_SetCmd is %d \r\n",nTrackerCMD.nCmd);
    }

    // Renew YOLO Queue (State!=Tracking)
    if(GLB_TrackerRes.nTrackerStatus != 3)
    {
        g_prev.clear();
        g_prev.reserve(ntargetnum);
        for (int i = 0; i < ntargetnum; i++) {
            int x0 = GLB_Objects[i].xmin;
            int y0 = GLB_Objects[i].ymin;
            int ww = GLB_Objects[i].w;
            int hh = GLB_Objects[i].h;
            YoloBox box;
            box.x = x0;
            box.y = y0;
            box.w = ww;
            box.h = hh;
            box.id = GLB_Objects[i].track_id;
            g_prev.push_back(box);
        }
    }

    // =================================== Start Tracking ===================================
    // 快照共享变量（TCP 线程可能正在写入）
    int local_select_flag;
    bool local_is_track;
    int local_yolo_num_id;
    int local_coord_x, local_coord_y;
    bool local_unlock_req;
    pthread_mutex_lock(&g_select_mutex);
    local_select_flag = select_flag;
    local_is_track = IS_TRACK;
    local_yolo_num_id = yolo_num_id;
    local_coord_x = Coordinate_X;
    local_coord_y = Coordinate_Y;
    local_unlock_req = g_unlock_requested;
    g_unlock_requested = false;  // 消费一次性请求
    pthread_mutex_unlock(&g_select_mutex);

    // 【客户端 UNLOCK 请求处理】
    // 只有通过 Tracker_SetCmd(CMD_ID_UNLOCK) 才能强制让 KCF 退出跟踪态。
    if (local_unlock_req && GLB_TrackerRes.nTrackerStatus == 3)
    {
        Tracker_Cmd nTrackerCMD;
        memset(&nTrackerCMD, 0, sizeof(nTrackerCMD));
        nTrackerCMD.nCmd = CMD_ID_UNLOCK;
        nTrackerCMD.nLockMod = LOCK_MOD_MANUAL;
        nTrackerCMD.nFrameId = nFrmCnt;
        Tracker_SetCmd(nTrackerCMD);
        printf("[INFO] Track unlocked by client request\n");
    }

    if(GLB_TrackerRes.nTrackerStatus != 3 && nFrmCnt > track_frame_cnt && local_is_track)
    // if(GLB_TrackerRes.nTrackerStatus != 3 &&  IS_TRACK )
    {   

        YoloBox sorted_box = { -1, -1, -1, -1, -1 }; // 初始化为空框

        // Select the target (By ID or XY or Velocity)
        if (local_select_flag == 0) // By Velocity
        {
            // ================================ velocity sort ================================
            if (bbox_norm_last.empty()) 
            {
            convertToYoloNorm(ntargetnum, width, height, bbox_norm_last);
            photo_last = inputMat.clone();
            out_boxes = bbox_norm_last;
            // convertToXYWH(out_boxes, width, height, g_prev);
            TRACK =  false;
            } 
            else 
            {
                convertToYoloNorm(ntargetnum, width, height, bbox_norm_now);
                bool ok = rankObjectsBySpeed(photo_last, bbox_norm_last, inputMat, bbox_norm_now, out_boxes);
                convertToXYWH(out_boxes, width, height, g_prev);
                bbox_norm_last = bbox_norm_now;
                photo_last = inputMat.clone();
                if(ok && nFrmCnt > velocity_track_frame_cnt && !g_prev.empty()){sorted_box = g_prev[0]; TRACK =  true;}
                else{TRACK =  false;}
            }

        }
        else if (local_select_flag == 1) // By XY
        {
            sorted_box = getBoxByXY(local_coord_x, local_coord_y);
            if (sorted_box.x != -1) {
                TRACK = true;
                printf("[SEL] XY(%d,%d) -> box(%d,%d,%d,%d) id=%d\n",
                       local_coord_x, local_coord_y,
                       sorted_box.x, sorted_box.y, sorted_box.w, sorted_box.h, sorted_box.id);
            } else {
                TRACK = false;
                printf("[SEL] XY(%d,%d) -> no match in %d boxes\n",
                       local_coord_x, local_coord_y, (int)g_prev.size());
            }
        }
        else if (local_select_flag == 2) // By ID
        {
            sorted_box = getBoxById(local_yolo_num_id);
            if (sorted_box.x != -1) {
                TRACK = true;
                printf("[SEL] ID=%d -> box(%d,%d,%d,%d)\n",
                       local_yolo_num_id, sorted_box.x, sorted_box.y, sorted_box.w, sorted_box.h);
            } else {
                TRACK = false;
                printf("[SEL] ID=%d -> not found in %d boxes (ids:",
                       local_yolo_num_id, (int)g_prev.size());
                for (size_t i = 0; i < g_prev.size(); i++)
                    printf(" %d", g_prev[i].id);
                printf(")\n");
            }
        }
        else
        {
            std::cout << "select_flag error" << std::endl;
            TRACK =  false;
        }



        if(TRACK){
            float xx = sorted_box.x ;
            float yy = sorted_box.y ;
            float ww = sorted_box.w;
            float hh = sorted_box.h;

            // 边界裁剪：确保跟踪框不超出图像边界 (1920x1080)
            const int img_width = 1920;
            const int img_height = 1080;
            
            if (xx < 0) { ww += xx; xx = 0; }
            if (yy < 0) { hh += yy; yy = 0; }
            if (xx + ww > img_width) ww = img_width - xx;
            if (yy + hh > img_height) hh = img_height - yy;
            
            // 检查裁剪后框是否有效
            if (ww <= 20 || hh <= 20) {
                std::cout << "[WARN] Box too small or invalid after clamping, skip tracking!" << std::endl;
                TRACK = false;
            }

            // === KCF 段错误防护 ===
            // libTrackerLib 的 KCF 在目标框周围提取子窗口做相关滤波
            // 需确保 margin = dim*PAD+BUF < img/2，否则持续缩小直到满足
            if (TRACK) {
                const float KCF_PAD = 1.75f;
                const float SAFE_BUF = 10.0f;
                const float max_w = (img_width  * 0.5f - SAFE_BUF) / KCF_PAD;
                const float max_h = (img_height * 0.5f - SAFE_BUF) / KCF_PAD;

                // 等比缩小到 margin 安全范围内
                if (ww > max_w || hh > max_h) {
                    float s = std::min(max_w / ww, max_h / hh);
                    float cx = xx + ww * 0.5f;
                    float cy = yy + hh * 0.5f;
                    ww *= s;
                    hh *= s;
                    xx = cx - ww * 0.5f;
                    yy = cy - hh * 0.5f;
                    printf("[INFO] KCF safety: box scaled to %dx%d\n", (int)ww, (int)hh);
                }

                // 再次边界裁剪（缩放可能改变了位置）
                if (xx < 0) { ww += xx; xx = 0; }
                if (yy < 0) { hh += yy; yy = 0; }
                if (xx + ww > img_width) ww = img_width - xx;
                if (yy + hh > img_height) hh = img_height - yy;
            }

            if(TRACK) {
            std::cout << "======================================="
            << "StartTracking: xx=" << xx 
            << " yy=" << yy 
            << " ww=" << ww 
            << " hh=" << hh
            <<  "start Tracking!" << std::endl;

            Tracker_Cmd nTrackerCMD;
            memset(&nTrackerCMD, 0, sizeof(nTrackerCMD));
            nTrackerCMD.nCmd = CMD_ID_LOCK; // 跟踪锁定目标
            nTrackerCMD.nLockMod = LOCK_MOD_MANUAL; // 手动锁定
            nTrackerCMD.nTargetId = (unsigned short)sorted_box.id;
            nTrackerCMD.nLock_x = (unsigned short)std::max(0.0f, std::min(xx, (float)(img_width - 1)));
            nTrackerCMD.nLock_y = (unsigned short)std::max(0.0f, std::min(yy, (float)(img_height - 1)));
            nTrackerCMD.nLock_w = (unsigned short)std::max(1.0f, std::min(ww, (float)img_width));
            nTrackerCMD.nLock_h = (unsigned short)std::max(1.0f, std::min(hh, (float)img_height));
            nTrackerCMD.nFrameId = nFrmCnt;

            Tracker_SetCmd(nTrackerCMD);
            printf("Tracker_SetCmd LOCK id=%d box=(%d,%d,%d,%d)\n",
                   sorted_box.id, (int)xx, (int)yy, (int)ww, (int)hh);

            // 锁定指令已发，清除 IS_TRACK 防止下一帧重复锁定
            pthread_mutex_lock(&g_select_mutex);
            IS_TRACK = false;
            pthread_mutex_unlock(&g_select_mutex);

            }  // end if(TRACK) - 边界检查后
        }
        

    }

    //regist_time
    auto t1 = high_resolution_clock::now();

    //start_tracking_or_detecting
    Tracker_CVMat_UpdateStatus(b, width, height, tResCallBack);

    //tracking lost,reset flags
    ResetAllStatesOnTrackLost(GLB_TrackerRes.nTrackerStatus, nFrmCnt, true);

    //tracking_time_or_detecting_time
    auto t2 = high_resolution_clock::now();
    
    drawBoxesByStatus(inputMat);

    // ---------------------------------------------------------------------
    // 【外部桥接】上报当前帧跟踪结果
    //   - 跟踪态(status==3) 时：内部会驱动伺服跟随目标
    //   - 总是：把状态报文通过 TCP 推给客户端
    // 具体行为见 tracker_bridge.c，本文件不关心。
    // ---------------------------------------------------------------------
    tracker_bridge_report(GLB_TrackerRes.nTrackerStatus,
                          (int)GLB_TrackerRes.xmin, (int)GLB_TrackerRes.ymin,
                          (int)GLB_TrackerRes.w,    (int)GLB_TrackerRes.h,
                          width, height, ntargetnum);

    // 每帧把 YOLO 目标列表发给客户端（用于客户端双击选目标）
    tracker_bridge_send_targets(GLB_Objects, ntargetnum);

    // =================================== 输出 ===================================
    // // 检测信息：
    // std::cout << "ntargetnum: " << ntargetnum << std::endl;
    nFrmCnt++;
    if (nFrmCnt % 30 == 0)
        printf("frm=%d status=%d box=(%.0f,%.0f,%.0f,%.0f)\n",
               nFrmCnt, GLB_TrackerRes.nTrackerStatus,
               GLB_TrackerRes.xmin, GLB_TrackerRes.ymin,
               GLB_TrackerRes.w, GLB_TrackerRes.h);

}


// // V2
// void processRGBData(unsigned char* pData, int width, int height){
//     // 记录当前跟踪目标信息（静态变量，在选择目标时更新）
//     static int current_yolo_num_id = -1;     // 当前跟踪的目标ID
//     static int current_Coordinate_X = 0;       // 当前跟踪的目标X坐标
//     static int current_Coordinate_Y = 0;       // 当前跟踪的目标Y坐标
    
//     std::cout<<"==================================================="<<std::endl;
    
//     // 根据 select_flag 打印不同的信息
//     if (select_flag == 2) {
//         // ID模式：打印当前跟踪的目标ID
//         std::cout << "[Mode: ID] Current Target ID: " << current_yolo_num_id << std::endl;
//     } else if (select_flag == 1) {
//         // XY模式：打印当前选择的坐标
//         std::cout << "[Mode: XY] Target Position: (" << current_Coordinate_X << ", " << current_Coordinate_Y << ")" << std::endl;
//     } else {
//         std::cout << "[Mode: Velocity]" << std::endl;
//     }


//     using namespace std::chrono;


//     // first_time_tag
//     auto t0 = std::chrono::high_resolution_clock::now();

//     cv::Mat inputMat(height, width, CV_8UC3, pData);
//     // printf("inputmat ok.\n");
    
//     // 【内存优化】使用静态 Mat 避免每帧重复分配内存
//     static cv::Mat b;
//     if (b.empty() || b.cols != 1920 || b.rows != 1080) {
//         b.create(1080, 1920, CV_8UC3);
//     }
    
//     // 【说明】KCF 使用 HOG 特征（基于梯度），不需要特定的颜色通道顺序
//     // 所以不需要 RGB→BGR 转换，直接 resize 即可
//     cv::resize(inputMat, b, cv::Size(1920, 1080), 0, 0, cv::INTER_LINEAR);
//     // printf("resize ok.\n");
//     if (b.empty()) {
//         std::cerr << "b is empty  zzzzzzzzzzzzzzzzzzzzzzzzzzzz \n" <<std::endl;

//     }
//     else{
//         std::cerr << "b size: " << b.size() << std::endl;

//     }
//     if (height == 1088) height = 1080;
//     // 【重要】使用 resize 后的图像尺寸，而不是原始输入尺寸
//     // 因为图像 b 已经被 resize 到 1920x1080，传入跟踪库的尺寸参数必须一致
//     int VIDEO_IMG_WIDTH = 1920;   // resize 后的宽度
//     int VIDEO_IMG_HEIGHT = 1080;  // resize 后的高度

//     // start initializing / detecting / tracking
//     if (0 == nFrmCnt)
//     {  
//         printf("Tracker_Register init.\n");
//         Tracker_Register(); // 跟踪器 注册及内存分配
//         // GLB_TrackerRes.nTrackerStatus = 1; // 强制设置为状态，测试用
//         printf("Tracker_Register is ok.\n");
//     }
//     if (1 == nFrmCnt)
//     {
//         Tracker_Cmd nTrackerCMD;
        
//         nTrackerCMD.nCmd = CMD_ID_UNLOCK; // 跟踪解锁
//         nTrackerCMD.nLockMod = LOCK_MOD_MANUAL; // 手动锁定
//         nTrackerCMD.nFrameId = nFrmCnt;

//         Tracker_SetCmd(nTrackerCMD);  
//         // GLB_TrackerRes.nTrackerStatus = 2; // 强制设置为跟踪状态，测试用
//         printf("Tracker_SetCmd is %d \r\n",nTrackerCMD.nCmd);
//     }



//     if (5 == nFrmCnt)
//     {
//         Tracker_Cmd nTrackerCMD;
        
//         nTrackerCMD.nCmd = CMD_ID_LOCK; // 跟踪锁定目标
//         nTrackerCMD.nLockMod = LOCK_MOD_MANUAL; // 手动锁定

//         nTrackerCMD.nLock_x = 1000; // 锁定目标的左上角坐标X
//         nTrackerCMD.nLock_y = 400; // 锁定目标的左上角坐标Y
//         nTrackerCMD.nLock_w = 200;  // 锁定目标的宽
//         nTrackerCMD.nLock_h = 200;  // 锁定目标的高
//         nTrackerCMD.nFrameId = nFrmCnt;

//         Tracker_SetCmd(nTrackerCMD);
//         // GLB_TrackerRes.nTrackerStatus = 3; // 强制设置为跟踪状态，测试用
//         printf("Tracker_SetCmd is %d \r\n",nTrackerCMD.nCmd);
//     }

//     //start_tracking_or_detecting
//     Tracker_CVMat_UpdateStatus(b, VIDEO_IMG_WIDTH, VIDEO_IMG_HEIGHT, tResCallBack);


//     drawBoxesByStatus(inputMat);

//     nFrmCnt++;
//     printf("nFrmCnt=%d\n", nFrmCnt);

//     // 跟踪信息：
//     std::cout << "GLB_TrackerRes.nTrackerStatus: " << GLB_TrackerRes.nTrackerStatus << std::endl;
//     std::cout << "GLB_TrackerRes.frame_id: " << GLB_TrackerRes.frame_id << std::endl;
//     std::cout << "GLB_TrackerRes.xmin: " << GLB_TrackerRes.xmin << std::endl;
//     std::cout << "GLB_TrackerRes.ymin: " << GLB_TrackerRes.ymin << std::endl;
//     std::cout << "GLB_TrackerRes.w: " << GLB_TrackerRes.w << std::endl;
//     std::cout << "GLB_TrackerRes.h: " << GLB_TrackerRes.h << std::endl;
//     std::cout << "GLB_TrackerRes.ntargetnum: " << GLB_TrackerRes.ntargetnum << std::endl;
//     std::cout << "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz " <<  std::endl;

// }





void convertToYoloNorm(int ntargetnum, int width, int height,
                       std::vector<YoloBox_norm>& bbox_norm_now)
{
    bbox_norm_now.clear();
    bbox_norm_now.reserve(ntargetnum);

    int W = width;
    int H = height;

    for (int i = 0; i < ntargetnum; ++i) {
        int x0 = GLB_Objects[i].xmin;
        int y0 = GLB_Objects[i].ymin;
        int ww = GLB_Objects[i].w;
        int hh = GLB_Objects[i].h;

        // skip invalid
        if (ww <= 0 || hh <= 0) continue;

        // clamp to image
        int x1 = x0; if (x1 < 0) x1 = 0; if (x1 > W) x1 = W;
        int y1 = y0; if (y1 < 0) y1 = 0; if (y1 > H) y1 = H;
        int x2 = x0 + ww; if (x2 < 0) x2 = 0; if (x2 > W) x2 = W;
        int y2 = y0 + hh; if (y2 < 0) y2 = 0; if (y2 > H) y2 = H;

        int bw = x2 - x1;
        int bh = y2 - y1;
        if (bw <= 0 || bh <= 0) continue;

        // pixel center -> normalized [0,1]
        float cx = (x1 + 0.5f * bw) / (float)W;
        float cy = (y1 + 0.5f * bh) / (float)H;
        float nw =  bw / (float)W;
        float nh =  bh / (float)H;

        // clamp to [0,1]
        if (cx < 0.f) cx = 0.f; if (cx > 1.f) cx = 1.f;
        if (cy < 0.f) cy = 0.f; if (cy > 1.f) cy = 1.f;
        if (nw < 0.f) nw = 0.f; if (nw > 1.f) nw = 1.f;
        if (nh < 0.f) nh = 0.f; if (nh > 1.f) nh = 1.f;

        YoloBox_norm b;
        b.cx = cx; b.cy = cy; b.w = nw; b.h = nh;
        b.id = GLB_Objects[i].track_id;

        bbox_norm_now.push_back(b);
    }
}
void convertToXYWH(const std::vector<YoloBox_norm>& out_boxesa,
                   int img_width, int img_height,
                   std::vector<YoloBox>& g_prev)
{
    g_prev.clear();
    g_prev.reserve(out_boxesa.size());

    int W = img_width;
    int H = img_height;
    if(out_boxesa.size() == 0){

        // oss_targets << "{}";
    }else
    {
        for (size_t i = 0; i < out_boxesa.size(); ++i) {
            const auto& b = out_boxesa[i];

            // 恢复为像素尺度
            float cx = b.cx * W;
            float cy = b.cy * H;
            float ww = b.w  * W;
            float hh = b.h  * H;

            // 左上角
            int x = static_cast<int>(cx - ww * 0.5f);
            int y = static_cast<int>(cy - hh * 0.5f);
            int w = static_cast<int>(ww);
            int h = static_cast<int>(hh);

            // clamp 边界
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x + w > W) w = W - x;
            if (y + h > H) h = H - y;
            if (w <= 0 || h <= 0) continue;

            // 填入输出容器
            YoloBox bb;
            bb.x = x;
            bb.y = y;
            bb.w = w;
            bb.h = h;
            bb.id = b.id;

            g_prev.push_back(bb);
        }
    }
    // std::cout << "[DEBUG] Data prepare: " << oss_targets.str() << std::endl;
}


YoloBox getBoxById(int id) {
    for (const auto& box : g_prev) {
        if (box.id == id) {
            return box;  // 找到后直接返回
        }
    }
    return { -1, -1, -1, -1, -1 }; // 没找到
}
YoloBox getBoxByXY(float x, float y) {
    // 1) 精确匹配：点击落在框内
    for (const auto& box : g_prev) {
        if (x >= box.x && x <= box.x + box.w &&
            y >= box.y && y <= box.y + box.h) {
            return box;
        }
    }
    // 2) 容差匹配：找距离点击位置最近的框中心（容差 200 像素）
    float bestDist = 200.0f * 200.0f;
    int bestIdx = -1;
    for (int i = 0; i < (int)g_prev.size(); i++) {
        float cx = g_prev[i].x + g_prev[i].w * 0.5f;
        float cy = g_prev[i].y + g_prev[i].h * 0.5f;
        float d = (cx - x) * (cx - x) + (cy - y) * (cy - y);
        if (d < bestDist) {
            bestDist = d;
            bestIdx = i;
        }
    }
    if (bestIdx >= 0) return g_prev[bestIdx];
    return { -1, -1, -1, -1, -1 };
}



static inline cv::Rect clampRect(int x, int y, int w, int h, int W, int H)
{
    cv::Rect r(x, y, w, h);
    cv::Rect bounds(0, 0, W, H);
    r = r & bounds; // 交集裁剪
    if (r.width <= 0 || r.height <= 0) return cv::Rect();
    return r;
}

static inline void drawIdText(cv::Mat& img, int id, int x, int y, const cv::Scalar& color)
{
    std::ostringstream ss;
    ss << id;

    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = std::max(0.5, std::min(1.0, img.cols / 1280.0));
    int thickness = 2;

    // 左上角显示；太靠上就放到框内
    int tx = std::max(0, x);
    int ty = y - 6;
    if (ty < 15) ty = y + 18;

    // 黑底提高可读性
    int baseline = 0;
    cv::Size tsz = cv::getTextSize(ss.str(), fontFace, fontScale, thickness, &baseline);
    cv::Rect bg(tx, ty - tsz.height - baseline, tsz.width + 6, tsz.height + baseline + 6);
    bg = bg & cv::Rect(0, 0, img.cols, img.rows);
    if (bg.width > 0 && bg.height > 0) {
        cv::rectangle(img, bg, cv::Scalar(0,0,0), cv::FILLED);
    }

    cv::putText(img, ss.str(), cv::Point(tx + 3, ty - 3),
                fontFace, fontScale, color, thickness, cv::LINE_AA);
}

static inline void drawBoxesByStatus(cv::Mat& inputMat)
{
    if (inputMat.empty()) return;

    const int W = inputMat.cols;
    const int H = inputMat.rows;

    // BGR 颜色：三类（按队列分段）
    const std::array<cv::Scalar, 3> clsColors = {
        cv::Scalar(0, 255, 0),   // 绿
        cv::Scalar(0, 0, 255),   // 红
        cv::Scalar(255, 0, 0)    // 蓝
    };
    const cv::Scalar oneColorDet = cv::Scalar(0, 255, 255);   // 黄：非 select_flag==0 的检测框
    const cv::Scalar trackColor  = cv::Scalar(0, 255, 0);     // 纯绿：跟踪框

    int detThick   = std::max(2, W / 640);
    int trackThick = std::max(6, W / 320);  // 跟踪框加粗，1920宽时=6

    // ===================== 检测状态：nTrackerStatus == 2，用 g_prev，画 id =====================
    if (GLB_TrackerRes.nTrackerStatus == 2)
    {
        const int n = (int)g_prev.size();
        if (n <= 0) return;

        if (select_flag == 0)
        {
            // k = min(3, n)，队列按顺序均分为 k 段
            const int k = std::min(3, n);

            for (int i = 0; i < n; ++i)
            {
                const auto& b = g_prev[i];

                cv::Rect r = clampRect(b.x, b.y, b.w, b.h, W, H);
                if (r.width <= 0 || r.height <= 0) continue;

                // 连续均分：cid ∈ [0, k-1]
                int cid = (i * k) / n;
                cid = (cid < 0) ? 0 : (cid > k - 1 ? k - 1 : cid);

                const cv::Scalar& c = clsColors[cid];
                cv::rectangle(inputMat, r, c, detThick, cv::LINE_AA);
                drawIdText(inputMat, b.id, r.x, r.y, c);
            }
        }
        else
        {
            // 其他模式：统一一种颜色
            for (int i = 0; i < n; ++i)
            {
                const auto& b = g_prev[i];

                cv::Rect r = clampRect(b.x, b.y, b.w, b.h, W, H);
                if (r.width <= 0 || r.height <= 0) continue;

                cv::rectangle(inputMat, r, oneColorDet, detThick, cv::LINE_AA);
                drawIdText(inputMat, b.id, r.x, r.y, oneColorDet);
            }
        }
        return;
    }

    // ===================== 跟踪状态：nTrackerStatus == 3，用 GLB_TrackerRes.xywh，不画 id =====================
    if (GLB_TrackerRes.nTrackerStatus == 3)
    {
        cv::Rect r = clampRect(GLB_TrackerRes.xmin, GLB_TrackerRes.ymin,
                               GLB_TrackerRes.w, GLB_TrackerRes.h, W, H);
        if (r.width <= 0 || r.height <= 0) return;

        // cv::rectangle(inputMat, r, trackColor, trackThick, cv::LINE_AA);
        cv::rectangle(inputMat, r, trackColor, trackThick, cv::LINE_8);


        // std::this_thread::sleep_for(std::chrono::milliseconds(0));
        


        
        return;
    }


    // 其他状态：不画
}

// ===================== Track Lost -> Reset All States =====================
// 当上一帧处于 Tracking(3)，当前帧不再是 Tracking(!=3)，认为跟踪失败：恢复所有状态位
static inline void ResetAllStatesOnTrackLost(int cur_status, int frame_id, bool send_unlock)
{
    // 记录上一次状态
    static int last_status = -1;

    // 仅在 “3 -> 非3” 发生时触发恢复
    if (last_status == 3 && cur_status != 3)
    {
        std::cout << "[WARN] Track lost! status 3 -> " << cur_status
                  << ", reset all state flags." << std::endl;

        // ---- 1) 恢复控制位 / 标志位 ----
        is_last_frame_ok = false;
        TRACK = false;
        is_velocity_ok = false;

        // ---- 2) 清空速度排序/光流相关缓存 ----
        bbox_norm_last.clear();
        bbox_norm_now.clear();
        out_boxes.clear();
        photo_last.release();

        // ---- 3) 清空检测队列（避免拿旧框继续锁定）----
        g_prev.clear();

        pthread_mutex_lock(&g_select_mutex);
        IS_TRACK = false;
        pthread_mutex_unlock(&g_select_mutex);

        // -----------------------------------------------------------------
        // 【外部桥接】跟踪丢失：通知伺服归零停转
        // 伺服细节封装在 tracker_bridge.c
        // -----------------------------------------------------------------
        tracker_bridge_on_track_lost();
    }

    // 更新 last_status
    last_status = cur_status;
}