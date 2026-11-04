#ifndef WAVEPANEL_H
#define WAVEPANEL_H
#include <QWidget>
#include <QTimer>

class QCustomPlot;

class WavePanel : public QWidget
{
    Q_OBJECT
public:
    explicit WavePanel(QWidget *parent = nullptr);

    // 外部推数据用：只加数据，不重绘
    void addData(double x, double y);

private slots:
    void onReplotTimeout();

private:
    QCustomPlot *m_plot;
    QTimer      *m_replotTimer;
    double       m_time;   // 仅用于内部模拟数据
};
#endif // WAVEPANEL_H
