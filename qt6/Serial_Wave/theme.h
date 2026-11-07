#ifndef THEME_H
#define THEME_H

#include <QColor>

// ================= 波形图主题配色 =================
// 想换配色只改这里，重新编译即可。
// 每个宏展开成一个 QColor，可直接塞进 QPen / QBrush / setXXXColor。

// 背景（整块画布 + 坐标轴绘图区共用）
// 想要纯黑就用 QColor(0, 0, 0)；嫌太硬可以改成 QColor(30, 30, 30)（#1e1e1e）
#define THEME_BG            QColor(30, 30, 30)

// 坐标轴线、刻度线
#define THEME_AXIS          QColor(200, 200, 200)

// 刻度数字和轴标题
#define THEME_TEXT          QColor(220, 220, 220)

// 主网格 / 次网格 / 零线
#define THEME_GRID          QColor(60, 60, 60)
#define THEME_SUB_GRID      QColor(40, 40, 40)
#define THEME_ZERO_LINE     QColor(90, 90, 90)

// 曲线颜色（黑底上蓝色偏暗，用亮色）
#define THEME_CURVE         QColor(0, 255, 128)

// 图例（以后加图例时用）
#define THEME_LEGEND_BG     QColor(0, 0, 0, 180)
#define THEME_LEGEND_TEXT   QColor(255, 255, 255)

#endif // THEME_H
