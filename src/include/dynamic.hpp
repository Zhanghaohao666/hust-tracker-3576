#pragma once

// 先引入 <cmath>，再清理可能的 C 宏，避免 isnan/isinf/fpclassify 等冲突
#include <cmath>
// #ifdef isnan
// #undef isnan
// #endif
// #ifdef isinf
// #undef isinf
// #endif
// #ifdef finite
// #undef finite
// #endif
// #ifdef signbit
// #undef signbit
// #endif
// #ifdef fpclassify
// #undef fpclassify
// #endif

// 只包含所需 OpenCV 头（避免用 <opencv2/opencv.hpp> 引入过多依赖）
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/video.hpp>

#include <vector>
#include <chrono>
#include <algorithm>

template<typename T>
static inline T clamp11(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct YoloBox_norm { float cx, cy, w, h; int id; };              // 归一化 YOLO 框 [0,1]
struct Profile { double gray_ms{}, flow_ms{}, bg_ms{}, roi_ms{}; }; // 关键阶段计时



// // 入口：img1、boxes1、img2、boxes2
// // 输出：true/false；out_box 为“全局最大速度”的 YOLO 归一化框（来自 boxes2）
// inline bool maxSpeedObject(
//     const cv::Mat& img1, const std::vector<YoloBox>& /*boxes1*/,
//     const cv::Mat& img2, const std::vector<YoloBox>& boxes2,
//     YoloBox& out_box, Profile* prof = nullptr
// ) {
//     if (img1.empty() || img2.empty() || boxes2.empty()) return false;

//     // ---------- 参数 ----------
//     constexpr int   TARGET_WIDTH  = 1080;
//     constexpr float REGION_SCALE  = 0.922f;   // ≈ sqrt(0.85)
//     constexpr float SHAKE_THRESH  = 0.5f;
//     constexpr int   SS            = 1;        // 稀疏采样步长
//     constexpr float EPS           = 1e-6f;

//     // ---------- 统一缩放 ----------
//     const int w0 = img1.cols, h0 = img1.rows;
//     const float scale = static_cast<float>(TARGET_WIDTH) / std::max(1, w0);
//     const int nw = TARGET_WIDTH;
//     const int nh = std::max(1, static_cast<int>(std::round(h0 * scale)));

//     cv::Mat r1, r2, g1, g2;
//     auto t0 = std::chrono::high_resolution_clock::now();
//     cv::resize(img1, r1, {nw, nh}, 0, 0, cv::INTER_LINEAR);
//     cv::resize(img2, r2, {nw, nh}, 0, 0, cv::INTER_LINEAR);
//     cv::cvtColor(r1, g1, cv::COLOR_BGR2GRAY);
//     cv::cvtColor(r2, g2, cv::COLOR_BGR2GRAY);
//     auto t1 = std::chrono::high_resolution_clock::now();

//     // ---------- 光流：Farneback（不依赖 opencv_contrib） ----------
//     // cv::Mat flow; // CV_32FC2
//     // const double pyr_scale = 0.5;
//     // const int    levels    = 3;
//     // const int    winsize   = 15;
//     // const int    iterations= 3;
//     // const int    poly_n    = 5;
//     // const double poly_sigma= 1.1;
//     // const int    flags     = 0;
//     // cv::calcOpticalFlowFarneback(g1, g2, flow,
//     //     pyr_scale, levels, winsize, iterations, poly_n, poly_sigma, flags);
//    cv::Mat flow; // CV_32FC2 — dense flow approximated via fast PyrLK on a grid

// // 1) Track a sparse grid of points with PyrLK (available in core OpenCV)
// const int step = 8; // larger = faster, coarser flow
// std::vector<cv::Point2f> p0; p0.reserve(((g1.cols+step-1)/step)*((g1.rows+step-1)/step));
// for (int y = step/2; y < g1.rows; y += step)
//     for (int x = step/2; x < g1.cols; x += step)
//         p0.emplace_back((float)x, (float)y);

// std::vector<cv::Point2f> p1;
// std::vector<uchar> st;
// std::vector<float> err;
// cv::Size win(15,15);
// int maxLevel = 2;
// cv::TermCriteria tc(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 10, 0.03);
// cv::calcOpticalFlowPyrLK(g1, g2, p0, p1, st, err, win, maxLevel, tc, 0, 1e-3f);

// // 2) Write displacements into a coarse flow grid, then upsample to dense
// int gw = (g1.cols + step - 1) / step, gh = (g1.rows + step - 1) / step;
// cv::Mat flowSmall(gh, gw, CV_32FC2, cv::Scalar(0,0));
// cv::Mat hits(gh, gw, CV_32SC1, cv::Scalar(0));

// for (size_t i = 0; i < p0.size(); ++i) {
//     if (!st[i]) continue;
//     int gx = std::min((int)p0[i].x / step, gw - 1);
//     int gy = std::min((int)p0[i].y / step, gh - 1);
//     cv::Vec2f d(p1[i].x - p0[i].x, p1[i].y - p0[i].y);
//     flowSmall.at<cv::Vec2f>(gy, gx) += d;
//     hits.at<int>(gy, gx) += 1;
// }
// for (int y = 0; y < gh; ++y)
//     for (int x = 0; x < gw; ++x)
//         if (hits.at<int>(y,x) > 0)
//             flowSmall.at<cv::Vec2f>(y,x) /= (float)hits.at<int>(y,x);

// cv::resize(flowSmall, flow, g1.size(), 0, 0, cv::INTER_LINEAR);






//     auto t2 = std::chrono::high_resolution_clock::now();

//     const int H = nh, W = nw;
//     cv::Mat1b mask(H, W, uchar(255));

//     // YOLO -> 缩放后像素 Rect（裁边）
//     auto yolo_to_rect = [&](const YoloBox& b)->cv::Rect {
//         const float cx = b.cx * w0 * scale;
//         const float cy = b.cy * h0 * scale;
//         const float ww = b.w  * w0 * scale;
//         const float hh = b.h  * h0 * scale;
//         int x1 = static_cast<int>(std::round(cx - ww * 0.5f));
//         int y1 = static_cast<int>(std::round(cy - hh * 0.5f));
//         int x2 = static_cast<int>(std::round(cx + ww * 0.5f));
//         int y2 = static_cast<int>(std::round(cy + hh * 0.5f));
//         x1 = clamp11(x1, 0, W); y1 = clamp11(y1, 0, H);
//         x2 = clamp11(x2, 0, W); y2 = clamp11(y2, 0, H);
//         return cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2));
//     };

//     // 当前帧 boxes2 作为前景，抠出背景
//     for (const auto& b : boxes2) {
//         cv::Rect r = yolo_to_rect(b);
//         if (r.area() > 0) mask(r).setTo(uchar(0));
//     }

//     // ---------- 背景平均光流 ----------
//     double sumx = 0.0, sumy = 0.0; int cnt = 0;
//     for (int y = 0; y < H; y += SS) {
//         const uchar* mptr = mask.ptr<uchar>(y);
//         const cv::Vec2f* fptr = flow.ptr<cv::Vec2f>(y);
//         for (int x = 0; x < W; x += SS) {
//             if (mptr[x]) { sumx += fptr[x][0]; sumy += fptr[x][1]; ++cnt; }
//         }
//     }
//     float bg_fx = 0.f, bg_fy = 0.f;
//     if (cnt > 0) { bg_fx = static_cast<float>(sumx / cnt); bg_fy = static_cast<float>(sumy / cnt); }
//     const float bg_mag = std::hypot(bg_fx, bg_fy);
//     auto t3 = std::chrono::high_resolution_clock::now();

//     // ---------- 逐 ROI 计算速度差 & 角度差，选择最大 ----------
//     float best_mag_diff = -1.f;
//     int best_i = -1;

//     for (int i = 0; i < static_cast<int>(boxes2.size()); ++i) {
//         cv::Rect r = yolo_to_rect(boxes2[i]);
//         if (r.width <= 0 || r.height <= 0) continue;

//         const int cw = static_cast<int>(std::round(r.width  * REGION_SCALE));
//         const int ch = static_cast<int>(std::round(r.height * REGION_SCALE));
//         const int cx = r.x + r.width  / 2;
//         const int cy = r.y + r.height / 2;
//         cv::Rect core(cv::Point(cx - cw / 2, cy - ch / 2), cv::Size(cw, ch));
//         core &= r;
//         core &= cv::Rect(0, 0, W, H);
//         if (core.width <= 0 || core.height <= 0) continue;

//         double sx = 0.0, sy = 0.0; int n = 0;
//         for (int y = core.y; y < core.y + core.height; y += SS) {
//             const cv::Vec2f* fptr = flow.ptr<cv::Vec2f>(y);
//             for (int x = core.x; x < core.x + core.width; x += SS) {
//                 sx += fptr[x][0]; sy += fptr[x][1]; ++n;
//             }
//         }
//         if (n == 0) continue;

//         const float fx = static_cast<float>(sx / n);
//         const float fy = static_cast<float>(sy / n);
//         const float mag = std::hypot(fx, fy);
//         const float mag_diff = std::fabs(mag - bg_mag);

//         const float dot = fx * bg_fx + fy * bg_fy;
//         const float denom = std::max(mag * bg_mag, EPS);
//         const float coss = clamp11(std::fabs(dot / denom), -1.0f, 1.0f);
//         const float ang_diff = std::acos(coss) * 180.0f / static_cast<float>(CV_PI);

//         const float theta = (bg_mag > SHAKE_THRESH) ? 1.0f : 0.0f;
//         const bool valid = (ang_diff > theta) && (mag_diff > 0.5f || (mag_diff < 0.5f && ang_diff > 3.0f));
//         if (valid && mag_diff > best_mag_diff) { best_mag_diff = mag_diff; best_i = i; }
//     }
//     auto t4 = std::chrono::high_resolution_clock::now();

//     if (prof) {
//         prof->gray_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
//         prof->flow_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
//         prof->bg_ms   = std::chrono::duration<double, std::milli>(t3 - t2).count();
//         prof->roi_ms  = std::chrono::duration<double, std::milli>(t4 - t3).count();
//     }

//     if (best_i < 0) return false;
//     out_box = boxes2[best_i];
//     return true;
// }


// // 1014 maxspeed opt flow
// // 入口：img1、boxes1、img2、boxes2
// // 输出：true/false；out_box 为“全局最大速度”的 YOLO 归一化框（来自 boxes2）
// inline bool maxSpeedObject(
//     const cv::Mat& img1, const std::vector<YoloBox>& /*boxes1*/,
//     const cv::Mat& img2, const std::vector<YoloBox>& boxes2,
//     YoloBox& out_box, Profile* prof = nullptr
// ) {
//     if (img1.empty() || img2.empty() || boxes2.empty()) return false;

//     // ---------- 参数 ----------
//     constexpr int   TARGET_WIDTH  = 1080;
//     constexpr float REGION_SCALE  = 0.922f;   // ≈ sqrt(0.85)
//     constexpr float SHAKE_THRESH  = 0.5f;
//     constexpr int   SS            = 1;        // 稀疏采样步长
//     constexpr float EPS           = 1e-6f;

//     // 估计帧间隔与检测滞后（毫秒）
//     constexpr double FRAME_DT_MS       = 33.333; // 假定 30 FPS，如有实际 FPS 可替换
//     constexpr double DETECT_OFFSET_MS  = 12.0;   // 额外滞后偏移量（可按实际系统校准）

//     // ---------- 统一缩放 ----------
//     const int w0 = img1.cols, h0 = img1.rows;
//     const float scale = static_cast<float>(TARGET_WIDTH) / std::max(1, w0);
//     const int nw = TARGET_WIDTH;
//     const int nh = std::max(1, static_cast<int>(std::round(h0 * scale)));

//     cv::Mat r1, r2, g1, g2;
//     auto t0 = std::chrono::high_resolution_clock::now();
//     cv::resize(img1, r1, {nw, nh}, 0, 0, cv::INTER_LINEAR);
//     cv::resize(img2, r2, {nw, nh}, 0, 0, cv::INTER_LINEAR);
//     cv::cvtColor(r1, g1, cv::COLOR_BGR2GRAY);
//     cv::cvtColor(r2, g2, cv::COLOR_BGR2GRAY);
//     auto t1 = std::chrono::high_resolution_clock::now();

//     // ---------- 光流：用 PyrLK 近似稠密光流 ----------
//     cv::Mat flow; // CV_32FC2 — dense flow approximated via fast PyrLK on a grid
//     const int step = 8; // larger = faster, coarser flow
//     std::vector<cv::Point2f> p0; p0.reserve(((g1.cols+step-1)/step)*((g1.rows+step-1)/step));
//     for (int y = step/2; y < g1.rows; y += step)
//         for (int x = step/2; x < g1.cols; x += step)
//             p0.emplace_back((float)x, (float)y);

//     std::vector<cv::Point2f> p1;
//     std::vector<uchar> st;
//     std::vector<float> err;
//     cv::Size win(15,15);
//     int maxLevel = 2;
//     cv::TermCriteria tc(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 10, 0.03);
//     cv::calcOpticalFlowPyrLK(g1, g2, p0, p1, st, err, win, maxLevel, tc, 0, 1e-3f);

//     // 2) 写入粗网格，再上采样到稠密
//     int gw = (g1.cols + step - 1) / step, gh = (g1.rows + step - 1) / step;
//     cv::Mat flowSmall(gh, gw, CV_32FC2, cv::Scalar(0,0));
//     cv::Mat hits(gh, gw, CV_32SC1, cv::Scalar(0));

//     for (size_t i = 0; i < p0.size(); ++i) {
//         if (!st[i]) continue;
//         int gx = std::min((int)p0[i].x / step, gw - 1);
//         int gy = std::min((int)p0[i].y / step, gh - 1);
//         cv::Vec2f d(p1[i].x - p0[i].x, p1[i].y - p0[i].y);
//         flowSmall.at<cv::Vec2f>(gy, gx) += d;
//         hits.at<int>(gy, gx) += 1;
//     }
//     for (int y = 0; y < gh; ++y)
//         for (int x = 0; x < gw; ++x)
//             if (hits.at<int>(y,x) > 0)
//                 flowSmall.at<cv::Vec2f>(y,x) /= (float)hits.at<int>(y,x);

//     cv::resize(flowSmall, flow, g1.size(), 0, 0, cv::INTER_LINEAR);

//     auto t2 = std::chrono::high_resolution_clock::now();

//     const int H = nh, W = nw;
//     cv::Mat1b mask(H, W, uchar(255));

//     // YOLO -> 缩放后像素 Rect（裁边）
//     auto yolo_to_rect = [&](const YoloBox& b)->cv::Rect {
//         const float cx = b.cx * w0 * scale;
//         const float cy = b.cy * h0 * scale;
//         const float ww = b.w  * w0 * scale;
//         const float hh = b.h  * h0 * scale;
//         int x1 = static_cast<int>(std::round(cx - ww * 0.5f));
//         int y1 = static_cast<int>(std::round(cy - hh * 0.5f));
//         int x2 = static_cast<int>(std::round(cx + ww * 0.5f));
//         int y2 = static_cast<int>(std::round(cy + hh * 0.5f));
//         x1 = clamp11(x1, 0, W); y1 = clamp11(y1, 0, H);
//         x2 = clamp11(x2, 0, W); y2 = clamp11(y2, 0, H);
//         return cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2));
//     };

//     // 当前帧 boxes2 作为前景，抠出背景
//     for (const auto& b : boxes2) {
//         cv::Rect r = yolo_to_rect(b);
//         if (r.area() > 0) mask(r).setTo(uchar(0));
//     }

//     // ---------- 背景平均光流 ----------
//     double sumx = 0.0, sumy = 0.0; int cnt = 0;
//     for (int y = 0; y < H; y += SS) {
//         const uchar* mptr = mask.ptr<uchar>(y);
//         const cv::Vec2f* fptr = flow.ptr<cv::Vec2f>(y);
//         for (int x = 0; x < W; x += SS) {
//             if (mptr[x]) { sumx += fptr[x][0]; sumy += fptr[x][1]; ++cnt; }
//         }
//     }
//     float bg_fx = 0.f, bg_fy = 0.f;
//     if (cnt > 0) { bg_fx = static_cast<float>(sumx / cnt); bg_fy = static_cast<float>(sumy / cnt); }
//     const float bg_mag = std::hypot(bg_fx, bg_fy);
//     auto t3 = std::chrono::high_resolution_clock::now();

//     // ---------- 逐 ROI 计算速度差 & 角度差，选择最大 ----------
//     float best_mag_diff = -1.f;
//     int   best_i = -1;
//     float best_fx = 0.f, best_fy = 0.f; // 记录最佳 ROI 的平均光流向量

//     for (int i = 0; i < static_cast<int>(boxes2.size()); ++i) {
//         cv::Rect r = yolo_to_rect(boxes2[i]);
//         if (r.width <= 0 || r.height <= 0) continue;

//         const int cw = static_cast<int>(std::round(r.width  * REGION_SCALE));
//         const int ch = static_cast<int>(std::round(r.height * REGION_SCALE));
//         const int cx = r.x + r.width  / 2;
//         const int cy = r.y + r.height / 2;
//         cv::Rect core(cv::Point(cx - cw / 2, cy - ch / 2), cv::Size(cw, ch));
//         core &= r;
//         core &= cv::Rect(0, 0, W, H);
//         if (core.width <= 0 || core.height <= 0) continue;

//         double sx = 0.0, sy = 0.0; int n = 0;
//         for (int y = core.y; y < core.y + core.height; y += SS) {
//             const cv::Vec2f* fptr = flow.ptr<cv::Vec2f>(y);
//             for (int x = core.x; x < core.x + core.width; x += SS) {
//                 sx += fptr[x][0]; sy += fptr[x][1]; ++n;
//             }
//         }
//         if (n == 0) continue;

//         const float fx = static_cast<float>(sx / n);
//         const float fy = static_cast<float>(sy / n);
//         const float mag = std::hypot(fx, fy);
//         const float mag_diff = std::fabs(mag - bg_mag);

//         const float dot = fx * bg_fx + fy * bg_fy;
//         const float denom = std::max(mag * bg_mag, EPS);
//         const float coss = clamp11(std::fabs(dot / denom), -1.0f, 1.0f);
//         const float ang_diff = std::acos(coss) * 180.0f / static_cast<float>(CV_PI);

//         const float theta = (bg_mag > SHAKE_THRESH) ? 1.0f : 0.0f;
//         const bool valid = (ang_diff > theta) && (mag_diff > 0.5f || (mag_diff < 0.5f && ang_diff > 3.0f));
//         if (valid && mag_diff > best_mag_diff) {
//             best_mag_diff = mag_diff;
//             best_i = i;
//             best_fx = fx; // 保存该 ROI 的光流向量
//             best_fy = fy;
//         }
//     }
//     auto t4 = std::chrono::high_resolution_clock::now();

//     if (prof) {
//         prof->gray_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
//         prof->flow_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
//         prof->bg_ms   = std::chrono::duration<double, std::milli>(t3 - t2).count();
//         prof->roi_ms  = std::chrono::duration<double, std::milli>(t4 - t3).count();
//     }

//     if (best_i < 0) return false;

//     // 以 boxes2[best_i] 为基础，保持 W/H 不变，仅根据速度与检测时延修正中心 (cx,cy)
//     out_box = boxes2[best_i];

//     // —— 检测流程耗时（毫秒），加上滞后偏移量 ——
//     const double detect_ms  = std::chrono::duration<double, std::milli>(t4 - t0).count();
//     const double latency_ms = detect_ms + DETECT_OFFSET_MS;

//     // —— 以每帧位移 (best_fx,best_fy) 估计速度，并按时延外推位置 ——
//     // best_fx/best_fy 是在缩放后的像素坐标下的一帧位移；归一化后与原图等效
//     const double scale_frames = (FRAME_DT_MS > 1e-9) ? (latency_ms / FRAME_DT_MS) : 0.0;
//     const float dx_norm = static_cast<float>((best_fx / static_cast<float>(W)) * scale_frames);
//     const float dy_norm = static_cast<float>((best_fy / static_cast<float>(H)) * scale_frames);

//     out_box.cx += dx_norm;
//     out_box.cy += dy_norm;

//     // 边界限制到 [0,1]（手动约束，不改变 w/h）
//     if (out_box.cx < 0.f) out_box.cx = 0.f; else if (out_box.cx > 1.f) out_box.cx = 1.f;
//     if (out_box.cy < 0.f) out_box.cy = 0.f; else if (out_box.cy > 1.f) out_box.cy = 1.f;

//     return true;
// }


// 输出：按“相对速度(光流-背景光流)”从快到慢排序后的 boxes2（YOLO 归一化框序列）
// 备注：完全删除了“时间/位置矫正（延迟外推）”逻辑
inline bool rankObjectsBySpeed(
    const cv::Mat& img1, const std::vector<YoloBox_norm>& /*boxes1*/,
    const cv::Mat& img2, const std::vector<YoloBox_norm>& boxes2,
    std::vector<YoloBox_norm>& out_boxes,
    std::vector<float>*   out_speeds = nullptr,   // 可选：输出对应速度，单位≈像素/帧（在统一缩放后）
    Profile*              prof       = nullptr
) {
    out_boxes.clear();
    if (out_speeds) out_speeds->clear();
    if (img1.empty() || img2.empty() || boxes2.empty()) return false;

    // ---------- 参数 ----------
    constexpr int   TARGET_WIDTH  = 1080;
    constexpr float REGION_SCALE  = 0.922f;   // 中心收缩核
    constexpr int   SS            = 1;        // 样本步长（越大越快越粗）
    constexpr float EPS           = 1e-6f;

    // ---------- 统一缩放 ----------
    const int w0 = img1.cols, h0 = img1.rows;
    const float scale = static_cast<float>(TARGET_WIDTH) / std::max(1, w0);
    const int nw = TARGET_WIDTH;
    const int nh = std::max(1, static_cast<int>(std::round(h0 * scale)));

    cv::Mat r1, r2, g1, g2;
    auto t0 = std::chrono::high_resolution_clock::now();
    cv::resize(img1, r1, {nw, nh}, 0, 0, cv::INTER_LINEAR);
    cv::resize(img2, r2, {nw, nh}, 0, 0, cv::INTER_LINEAR);
    cv::cvtColor(r1, g1, cv::COLOR_BGR2GRAY);
    cv::cvtColor(r2, g2, cv::COLOR_BGR2GRAY);
    auto t1 = std::chrono::high_resolution_clock::now();

    // ---------- 稀疏 PyrLK → 稠密近似光流 ----------
    cv::Mat flow; // CV_32FC2
    const int step = 8; // 网格步长：数值越大→速度越快但更粗糙
    std::vector<cv::Point2f> p0; p0.reserve(((g1.cols+step-1)/step)*((g1.rows+step-1)/step));
    for (int y = step/2; y < g1.rows; y += step)
        for (int x = step/2; x < g1.cols; x += step)
            p0.emplace_back((float)x, (float)y);

    std::vector<cv::Point2f> p1;
    std::vector<uchar> st;
    std::vector<float> err;
    cv::Size win(15,15);
    int maxLevel = 2;
    cv::TermCriteria tc(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, 10, 0.03);
    cv::calcOpticalFlowPyrLK(g1, g2, p0, p1, st, err, win, maxLevel, tc, 0, 1e-3f);

    // 写入粗网格，再上采样为稠密场
    int gw = (g1.cols + step - 1) / step, gh = (g1.rows + step - 1) / step;
    cv::Mat flowSmall(gh, gw, CV_32FC2, cv::Scalar(0,0));
    cv::Mat hits(gh, gw, CV_32SC1, cv::Scalar(0));
    for (size_t i = 0; i < p0.size(); ++i) {
        if (!st[i]) continue;
        int gx = std::min((int)p0[i].x / step, gw - 1);
        int gy = std::min((int)p0[i].y / step, gh - 1);
        cv::Vec2f d(p1[i].x - p0[i].x, p1[i].y - p0[i].y);
        flowSmall.at<cv::Vec2f>(gy, gx) += d;
        hits.at<int>(gy, gx) += 1;
    }
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x)
            if (hits.at<int>(y,x) > 0)
                flowSmall.at<cv::Vec2f>(y,x) /= (float)hits.at<int>(y,x);
    cv::resize(flowSmall, flow, g1.size(), 0, 0, cv::INTER_LINEAR);
    auto t2 = std::chrono::high_resolution_clock::now();

    const int H = nh, W = nw;

    // ---------- 计算背景平均光流（检测框之外） ----------
    cv::Mat1b mask(H, W, uchar(255));
    auto yolo_to_rect = [&](const YoloBox_norm& b)->cv::Rect {
        const float cx = b.cx * w0 * scale;
        const float cy = b.cy * h0 * scale;
        const float ww = b.w  * w0 * scale;
        const float hh = b.h  * h0 * scale;
        int x1 = static_cast<int>(std::round(cx - ww * 0.5f));
        int y1 = static_cast<int>(std::round(cy - hh * 0.5f));
        int x2 = static_cast<int>(std::round(cx + ww * 0.5f));
        int y2 = static_cast<int>(std::round(cy + hh * 0.5f));
        x1 = clamp11(x1, 0, W); y1 = clamp11(y1, 0, H);
        x2 = clamp11(x2, 0, W); y2 = clamp11(y2, 0, H);
        return cv::Rect(cv::Point(x1, y1), cv::Point(x2, y2));
    };
    for (const auto& b : boxes2) {
        cv::Rect r = yolo_to_rect(b);
        if (r.area() > 0) mask(r).setTo(uchar(0));
    }
    double sumx = 0.0, sumy = 0.0; int cnt = 0;
    for (int y = 0; y < H; y += SS) {
        const uchar* mptr = mask.ptr<uchar>(y);
        const cv::Vec2f* fptr = flow.ptr<cv::Vec2f>(y);
        for (int x = 0; x < W; x += SS) {
            if (mptr[x]) { sumx += fptr[x][0]; sumy += fptr[x][1]; ++cnt; }
        }
    }
    const float bg_fx = (cnt>0) ? static_cast<float>(sumx/cnt) : 0.f;
    const float bg_fy = (cnt>0) ? static_cast<float>(sumy/cnt) : 0.f;
    auto t3 = std::chrono::high_resolution_clock::now();

    // ---------- 对每个 ROI 计算“相对速度”并排序 ----------
    struct Item { int idx; float speed; };
    std::vector<Item> items; items.reserve(boxes2.size());

    for (int i = 0; i < (int)boxes2.size(); ++i) {
        cv::Rect r = yolo_to_rect(boxes2[i]);
        if (r.width <= 0 || r.height <= 0) { items.push_back({i, 0.f}); continue; }

        const int cw = static_cast<int>(std::round(r.width  * REGION_SCALE));
        const int ch = static_cast<int>(std::round(r.height * REGION_SCALE));
        const int cx = r.x + r.width  / 2;
        const int cy = r.y + r.height / 2;
        cv::Rect core(cv::Point(cx - cw / 2, cy - ch / 2), cv::Size(cw, ch));
        core &= r;
        core &= cv::Rect(0, 0, W, H);
        if (core.width <= 0 || core.height <= 0) { items.push_back({i, 0.f}); continue; }

        double sx = 0.0, sy = 0.0; int n = 0;
        for (int y = core.y; y < core.y + core.height; y += SS) {
            const cv::Vec2f* fptr = flow.ptr<cv::Vec2f>(y);
            for (int x = core.x; x < core.x + core.width; x += SS) {
                sx += fptr[x][0]; sy += fptr[x][1]; ++n;
            }
        }
        if (n == 0) { items.push_back({i, 0.f}); continue; }

        const float fx = static_cast<float>(sx / n);
        const float fy = static_cast<float>(sy / n);

        // 关键：相机运动补偿后的“相对速度”
        const float rx = fx - bg_fx;
        const float ry = fy - bg_fy;
        const float speed = std::sqrt(std::max(rx*rx + ry*ry, 0.f));

        items.push_back({i, speed});
    }
    auto t4 = std::chrono::high_resolution_clock::now();

    // 按速度从大到小排序
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){
        return a.speed > b.speed;
    });

    out_boxes.reserve(items.size());
    if (out_speeds) out_speeds->reserve(items.size());
    for (const auto& it : items) {
        out_boxes.push_back(boxes2[it.idx]);   // 保持 YOLO 归一化 (cx,cy,w,h)
        if (out_speeds) out_speeds->push_back(it.speed);
    }

    if (prof) {
        prof->gray_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        prof->flow_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
        prof->bg_ms   = std::chrono::duration<double, std::milli>(t3 - t2).count();
        prof->roi_ms  = std::chrono::duration<double, std::milli>(t4 - t3).count();
    }

    return !out_boxes.empty();
}

