#ifndef RUNTRACKER_HPP
#define RUNTRACKER_HPP

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>
#include <chrono>
#include "../include/dynamic.hpp"
// #include "kcftracker.hpp"  // 依赖的KCF跟踪器头文件

// 使用的命名空间（建议在实现里用，头文件中最好不要全局 using）
using cv::Mat;
using cv::Rect;

// unsigned int ntargetnum = 0;
struct YoloBox { int x, y, w, h, id; }; 


 
// static std::vector<YoloBox_norm> bbox_norm; // ctf last frame yolo box
#ifdef __cplusplus
extern "C" {
#endif

// 声明你的函数
void processRGBData(unsigned char* data, int width, int height); 

#ifdef __cplusplus
}
#endif


// void processYUVData(unsigned char* yuvData, int width, int height);
// void processRGBData(unsigned char* pData, int width, int height);

YoloBox getBoxById(int id);

YoloBox getBoxByXY(float x, float y);

void convertToYoloNorm(int ntargetnum, int width, int height,
                       std::vector<YoloBox_norm>& bbox_norm);

void convertToXYWH(const std::vector<YoloBox_norm>& out_boxesa,
                   int img_width, int img_height,
                   std::vector<YoloBox>& g_prev);

static inline void drawBoxesByStatus(cv::Mat& inputMat);
static inline void ResetAllStatesOnTrackLost(int cur_status, int frame_id, bool send_unlock);


#endif // RUNTRACKER_HPP


