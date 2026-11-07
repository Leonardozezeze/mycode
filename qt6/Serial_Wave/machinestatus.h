#ifndef MACHINESTATUS_H
#define MACHINESTATUS_H

#include <QtNumeric>

// 解析层负责填这个结构体，界面只负责显示。
// 没有数据的字段保持 NaN，界面会显示 "--"。
struct MachineStatus
{
    double  crushRate      = qQNaN(); // 当前破碎率 %
    double  crushRateAvg   = qQNaN(); // 平均破碎率 %
    double  impurityRate   = qQNaN(); // 当前含杂率 %
    double  impurityRateAvg = qQNaN();// 平均含杂率 %
    double  rpm          = qQNaN();   // 电机实际转速 rpm
    double  rpmTarget    = qQNaN();   // 设定转速 rpm
    double  current      = qQNaN();   // 电机电流 A
    bool    running      = false;     // 电机运行中
    bool    alarm        = false;     // 报警状态
    quint64 frames       = 0;         // 已接收帧数
    quint64 errors       = 0;         // 错误帧数
    double  updateHz     = qQNaN();   // 实际数据更新频率
};

#endif // MACHINESTATUS_H
