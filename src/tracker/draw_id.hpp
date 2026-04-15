#include <algorithm>
#include <string>
// #include<omp.h>

// ---------------------------
// 0~9 的 7x5 点阵字库
// 每个数字占 7 行，每行 5 列，1=点亮，0=空
// ---------------------------
static const unsigned char DIGIT_FONT[10][7][5] = {
    { // 0
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,1,1},
        {1,0,1,0,1},
        {1,1,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    { // 1
        {0,0,1,0,0},
        {0,1,1,0,0},
        {1,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {1,1,1,1,1}
    },
    { // 2
        {0,1,1,1,0},
        {1,0,0,0,1},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,1,0,0,0},
        {1,1,1,1,1}
    },
    { // 3
        {1,1,1,1,0},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,1,0},
        {0,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    { // 4
        {0,0,0,1,0},
        {0,0,1,1,0},
        {0,1,0,1,0},
        {1,0,0,1,0},
        {1,1,1,1,1},
        {0,0,0,1,0},
        {0,0,0,1,0}
    },
    { // 5
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {0,0,0,0,1},
        {0,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    { // 6
        {0,1,1,1,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    { // 7
        {1,1,1,1,1},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0}
    },
    { // 8
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    { // 9
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,1},
        {0,0,0,0,1},
        {0,0,0,0,1},
        {0,1,1,1,0}
    }
};

// --------------------------------------------------------
// 绘制一个数字
// yPtr: Y 通道指针
// width, height: 图像宽高
// x, y: 左上角位置
// digit: 0~9
// scale: 缩放大小（每个点放大成 scale*scale）
// --------------------------------------------------------
void drawDigit(unsigned char* yPtr, int width, int height,
               int x, int y, int digit, int scale)
{
    if (digit < 0 || digit > 9) return;

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (DIGIT_FONT[digit][row][col]) {
                // 每个点扩展成 scale*scale 的小方块
                for (int dy = 0; dy < scale; ++dy) {
                    for (int dx = 0; dx < scale; ++dx) {
                        int px = x + col*scale + dx;
                        int py = y + row*scale + dy;
                        if (px >= 0 && px < width && py >= 0 && py < height) {
                            yPtr[py * width + px] = 255; // 白色
                        }
                    }
                }
            }
        }
    }
}

// --------------------------------------------------------
// 绘制一个整数 ID（支持多位）
// --------------------------------------------------------
void drawID(unsigned char* yPtr, int width, int height,
            int x, int y, int id, int scale)
{
    std::string text = std::to_string(id);
    int digitWidth = 5 * scale + 1; // 每个数字宽度 +1 间隔

    for (size_t i = 0; i < text.size(); i++) {
        int digit = text[i] - '0';
        int xOffset = x + i * digitWidth;
        drawDigit(yPtr, width, height, xOffset, y, digit, scale);
    }
}
void draw404(unsigned char* yPtr, int width, int height, int scale)
{
    int x = 15;  // 左上角横坐标
    int y = 15;  // 左上角纵坐标
    int value = 404;

    drawID(yPtr, width, height, x, y, value, scale);
}


void DrawRank(unsigned char* yPtr, int width, int height,
              int x, int y, int w, int h,  // 框的左上角与宽高
              int rank, int scale)
{
    // 将 rank 转成字符串（例如 1, 2, 3...）
    std::string text = std::to_string(rank);

    // 每个数字宽度（根据 scale 调整），+1 表示字符间距
    int digitWidth = 5 * scale + 1;

    // --- 计算右上角起始位置 ---
    // 右上角坐标：框的右边 - 整个文字长度
    int textWidth = (int)text.size() * digitWidth;
    int xStart = x + w - textWidth - 2;  // 向左偏 2 像素避免越界
    int yStart = y + 2;                  // 向下偏 2 像素防止贴边

    // --- 遍历每个数字绘制 ---
    for (size_t i = 0; i < text.size(); i++) {
        int digit = text[i] - '0';
        int xOffset = xStart + i * digitWidth;
        drawDigit(yPtr, width, height, xOffset, yStart, digit, scale);
    }
}

inline void drawBox(unsigned char* yPtr, int width, int height,
                    int x0, int y0, int x1, int y1, int thickness)
{
    // clamp
    x0 = std::max(0, x0); y0 = std::max(0, y0);
    x1 = std::min(width  - 1, x1);
    y1 = std::min(height - 1, y1);

    for (int t = 0; t < thickness; ++t) {
        int ty0 = std::max(0, y0 - t);
        int ty1 = std::min(height - 1, y1 + t);
        int tx0 = std::max(0, x0 - t);
        int tx1 = std::min(width  - 1, x1 + t);

        // 上下边
        #pragma omp parallel for
        for (int x = tx0; x <= tx1; ++x) {
            yPtr[ty0 * width + x] = 255;
            yPtr[ty1 * width + x] = 255;
        }

        // 左右边
        #pragma omp parallel for
        for (int y = ty0; y <= ty1; ++y) {
            yPtr[y * width + tx0] = 255;
            yPtr[y * width + tx1] = 255;
        }
    }
}