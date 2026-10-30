#!/usr/bin/env python3
"""
压电传感器冲击信号模拟器
模拟种子撞击传感器的信号 + 参数数据

数据协议：
timestamp,signal,grain_count,loss_rate,fan_rpm
"""

import serial
import time
import math
import sys
import random

class ImpactSignal:
    """压电传感器冲击信号生成器"""
    def __init__(self):
        self.last_impact_time = 0
        self.impact_interval_min = 500  # 最小间隔 500ms
        self.impact_interval_max = 3000  # 最大间隔 3000ms
        self.next_impact_time = random.randint(self.impact_interval_min, self.impact_interval_max)
        self.in_impact = False  # 是否正在冲击中
        self.impact_amplitude = 0
        self.impact_decay_time = 0

    def generate(self, timestamp, base_noise_level=5.0):
        """生成带噪声的冲击信号，返回 (signal, is_impact_start)"""
        # 基础噪声
        noise = random.gauss(0, base_noise_level)
        is_impact_start = False

        # 检查是否应该开始新的冲击
        if timestamp >= self.next_impact_time and not self.in_impact:
            # 开始新的冲击
            self.in_impact = True
            self.last_impact_time = timestamp
            is_impact_start = True

            # 下一次冲击时间（在本次冲击结束后）
            interval = random.randint(self.impact_interval_min, self.impact_interval_max)
            self.next_impact_time = timestamp + interval

            # 冲击参数
            self.impact_amplitude = random.uniform(80, 150)  # 冲击幅度
            self.impact_decay_time = random.uniform(50, 150)  # 衰减时间 (ms)

        if self.in_impact:
            time_since_impact = timestamp - self.last_impact_time
            impact_value = self.impact_amplitude * math.exp(-time_since_impact / self.impact_decay_time)

            # 冲击结束判断
            if time_since_impact > self.impact_decay_time * 3:
                self.in_impact = False

            return impact_value + noise, is_impact_start

        return noise, False

def main():
    if len(sys.argv) < 2:
        print("用法: python3 mock_serial.py <串口设备>")
        print("示例: python3 mock_serial.py /dev/ttyV0")
        sys.exit(1)

    port = sys.argv[1]
    baudrate = 115200

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"已连接到 {port} @ {baudrate} 波特率")
        print("数据格式: timestamp,signal,grain_count,loss_rate,fan_rpm")
        print("按 Ctrl+C 停止")
    except serial.SerialException as e:
        print(f"无法打开串口: {e}")
        sys.exit(1)

    timestamp = 0
    interval_ms = 10  # 10ms 间隔 (100Hz)

    # 参数初始化
    grain_count = 0  # 籽粒数量 = 累计冲击次数

    # 损失率和转速的平滑变化
    target_loss_rate = 2.0  # 目标损失率
    current_loss_rate = 2.0  # 当前损失率
    target_fan_rpm = 1500  # 目标转速
    current_fan_rpm = 1500  # 当前转速

    # 每 5-15 秒改变一次目标值
    next_target_change = random.randint(5000, 15000)

    impact_gen = ImpactSignal()

    try:
        while True:
            # 生成冲击信号
            signal, is_impact_start = impact_gen.generate(timestamp)

            # 每次新冲击开始，籽粒数量 +1
            if is_impact_start:
                grain_count += 1

            # 定期改变目标值
            if timestamp >= next_target_change:
                # 损失率目标在 1.7~2.4% 之间随机选择
                target_loss_rate = random.uniform(1.7, 2.4)
                # 转速目标与损失率成反比
                target_fan_rpm = 1500 * (2.0 / target_loss_rate) + random.uniform(-50, 50)
                target_fan_rpm = max(1200, min(1800, target_fan_rpm))

                # 下次改变时间
                next_target_change = timestamp + random.randint(5000, 15000)
                print(f"[{timestamp}ms] 目标变化: 损失率->{target_loss_rate:.2f}% 转速->{target_fan_rpm:.0f}RPM")

            # 当前值缓慢向目标值逼近（指数平滑）
            # alpha 越小，变化越慢越平滑
            alpha = 0.002  # 每 10ms 变化 0.2%，约 5 秒变化 63%
            current_loss_rate += alpha * (target_loss_rate - current_loss_rate)
            current_fan_rpm += alpha * (target_fan_rpm - current_fan_rpm)

            # 添加微小噪声（让曲线不那么死板）
            loss_rate = current_loss_rate + random.gauss(0, 0.02)
            fan_rpm = current_fan_rpm + random.gauss(0, 5)

            # 限制范围
            loss_rate = max(1.5, min(2.6, loss_rate))
            fan_rpm = max(1000, min(2000, fan_rpm))

            # 发送数据: timestamp,signal,grain_count,loss_rate,fan_rpm
            data = f"{timestamp},{signal:.2f},{grain_count:.1f},{loss_rate:.2f},{fan_rpm:.0f}\n"
            ser.write(data.encode('utf-8'))

            if timestamp % 1000 == 0:
                print(f"[{timestamp}ms] 信号:{signal:6.2f} 籽粒:{grain_count:6.1f} 损失率:{loss_rate:.2f}% 转速:{fan_rpm:.0f}")

            timestamp += interval_ms
            time.sleep(interval_ms / 1000.0)

    except KeyboardInterrupt:
        print("\n停止发送")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
