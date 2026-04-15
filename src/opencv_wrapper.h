#ifndef OPENCV_WRAPPER_H
#define OPENCV_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 在 NV12 图像上绘制文字
 * @param yuv_data NV12 数据指针
 * @param width 图像宽度
 * @param height 图像高度
 * @param text 要绘制的文字
 */
void draw_text_on_image(unsigned char *yuv_data, int width, int height,
                        const char *text);

#ifdef __cplusplus
}
#endif

#endif // OPENCV_WRAPPER_H
