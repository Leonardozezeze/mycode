#ifndef WAVEPANEL_H
#define WAVEPANEL_H

#include <QWidget>
#include <QVector>

#include "samplearchive.h"

class QCustomPlot;
class QTimer;
class QPushButton;
class QLabel;

class WavePanel : public QWidget
{
    Q_OBJECT
public:
    explicit WavePanel(QWidget *parent = nullptr);

    // 外部推数据用：一次传入所有通道的值（顺序与界面上的通道按钮一致）。
    // 所有通道都会归档；只绘制当前选中的通道，且不在这里重绘。
    // values 少于通道数时，缺的通道按 0 处理。
    void addData(double x, const QVector<double> &values);

    // 兼容单通道调用：只写第 0 通道，其余通道记 0
    void addData(double x, double y) { addData(x, QVector<double>{y}); }

    void clearData();

    // 已归档的采样点数（不受绘图窗口限制）
    qint64 sampleCount() const { return m_archive.count(); }
    // 最近一个采样的时间（秒）；没有数据时返回 0
    double lastTime() const;
    // 缓冲文件当前大小（字节）
    qint64 bufferSize() const;
    // 本次运行的缓冲文件路径（调试用；进程退出后自动删除）
    QString bufferFileName() const { return m_archive.bufferFileName(); }

    // 按扩展名导出全部数据（.json 走 JSON，其余走 CSV），不弹对话框。
    // 供“自动导出”等程序化场景使用；失败时返回 false。
    bool exportDataTo(const QString &path);

public slots:
    void exportDataDialog();   // 记录面板用：CSV / JSON 数据导出
    void exportPngDialog();    // 波形工具栏用：只导出 PNG 图片

private slots:
    void onReplotTimeout();
    void togglePause(bool paused);
    void toggleAutoY(bool enabled);

private:
    void setChannel(int channel);
    void followLatest(double x);
    void appendToGraph(double x, double y);
    void applyAutoY();
    // 从归档里按范围取数，替换绘图数据（暂停查看历史 / 恢复时回到最新 / 切通道）
    void loadRangeFromArchive(double left, double right);
    void followLatestFromArchive();
    void onViewRangeChanged(double lower, double upper);
    bool exportCsv(const QString &path);
    bool exportJson(const QString &path);
    bool exportPng(const QString &path);

    QCustomPlot *m_plot       = nullptr;
    QTimer      *m_replotTimer = nullptr;
    QPushButton *m_pauseBtn   = nullptr;
    QPushButton *m_autoYBtn   = nullptr;

    SampleArchive m_archive;   // 采样归档：小内存缓冲 + 临时文件落盘

    int    m_selectedChannel = 0;  // 当前绘制显示的是哪个通道
    bool   m_paused = false;
    bool   m_autoY  = false;
    bool   m_dirty  = false;      // 有新数据待重绘，空闲时不空转 replot
    bool   m_loadingView = false;  // 正在从归档重建绘图数据，忽略 rangeChanged 联动
};

#endif // WAVEPANEL_H
