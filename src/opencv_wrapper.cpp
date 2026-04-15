#include "opencv_wrapper.h"

extern "C" {
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
}

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include "omp.h"

extern "C" void draw_text_on_image(unsigned char* yuv_data,
                                   int width, int height,
                                   const char* text)
{
    if (!yuv_data || !text || width <=0 || height <=0) return;

    // NV12 转 BGR
    cv::Mat yuv(height + height/2, width, CV_8UC1, yuv_data);
    cv::Mat bgr;
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_NV12);

    // 绘制文字
    cv::putText(bgr, text,
                cv::Point(150, 150),           // 坐标
                cv::FONT_HERSHEY_SIMPLEX,    // 字体
                2.0,                         // 大小
                cv::Scalar(0, 255, 0),       // 绿色
                3);                          // 粗细

    // BGR 转回 I420（YUV420p）
    cv::Mat yuv_i420;
    cv::cvtColor(bgr, yuv_i420, cv::COLOR_BGR2YUV_I420);

    // 手动转换 I420 -> NV12
    unsigned char* pY = yuv_i420.data;
    unsigned char* pU = pY + width*height;
    unsigned char* pV = pU + width*height/4;

    // 拷贝 Y 平面
    memcpy(yuv_data, pY, width*height);

    // 转换 UV 平面
    unsigned char* pDstUV = yuv_data + width*height;
    for(int i = 0; i < height/2; i++) {
        for(int j = 0; j < width/2; j++) {
            pDstUV[i*width + j*2]     = pU[i*width/2 + j]; // U
            pDstUV[i*width + j*2 + 1] = pV[i*width/2 + j]; // V
        }
    }
}
