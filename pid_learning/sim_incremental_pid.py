import matplotlib.pyplot as plt

class IncrementalPID:
    """增量式PID:计算输出增量 Δu 并累加到上一次输出。

    公式:
        Δu = Kp*(e - e1) + Ki*e + Kd*(e - 2*e1 + e2)
        u  = u1 + Δu   (e1/e2 为前一次/前两次误差, u1 为上一次输出)

    相比位置式, 没有积分累加器, 因此天然无积分饱和(windup),
    输出限幅只需要直接截断, 无需反算退绕。
    """
    def __init__(self,Kp,Ki,Kd,dt,
                 output_max=100.0,output_min=-100.0):
        #控制增益
        self.Kp=Kp
        self.Ki=Ki
        self.Kd=Kd
        self.dt=dt
        #(执行边界)输出限幅
        self.output_max=output_max
        self.output_min=output_min
        #内部状态变量
        self._prev_error=0.0#上一次误差 e(k-1)
        self._prev_error2=0.0#上上次误差 e(k-2)
        self._prev_output=0.0#上一次输出 u(k-1)

        self.first_run=True

    def reset(self):
        self._prev_error=0.0
        self._prev_error2=0.0
        self._prev_output=0.0
        self.first_run=True
        print("PID内部状态已重置")

    def update(self,setpoint,measurement):
        error=setpoint-measurement
        #前两步缺失的历史误差按当前误差处理, 避免微分项突变
        if self.first_run:
            e1=error
            e2=error
            self.first_run=False
        else:
            e1=self._prev_error
            e2=self._prev_error2

        #增量式PID计算
        du=(self.Kp*(error-e1)
            +self.Ki*error
            +self.Kd*(error-2*e1+e2))

        #累加到上一次输出, 并做输出限幅(无需积分退绕)
        raw_output=self._prev_output+du
        if raw_output>self.output_max:
            control_signal=self.output_max
        elif raw_output<self.output_min:
            control_signal=self.output_min
        else:
            control_signal=raw_output

        #更新状态: 存限幅后的实际输出, 下次增量从真实输出上累加
        self._prev_error2=e1
        self._prev_error=error
        self._prev_output=control_signal
        return control_signal

    def set_gains(self,Kp=None,Ki=None,Kd=None):
        """运行时调整PID参数 方便调试"""
        if Kp is not None:
            self.Kp=Kp
        if Ki is not None:
            self.Ki=Ki
        if Kd is not None:
            self.Kd=Kd
        print(f"PID参数已经更新:Kp={self.Kp},Ki={self.Ki},Kd={self.Kd}")

if __name__=="__main__":
    pid=IncrementalPID(
        Kp=0.8,
        Ki=0.02,
        Kd=0.05,
        dt=0.01,
        output_max=100.0,
        output_min=-100.0
    )
    #系统变量
    setpoint=20.0#目标值
    time=10#仿真时间
    plant_state=0.0#当前值
    tau=0.5#系统惯性时间常数
    #数据记录
    timestamps=[]
    outputs=[]
    controls=[]
    #仿真循环
    n_steps=int(time/pid.dt)
    for i in range(n_steps):
        t=i*pid.dt
        timestamps.append(t)
        control_signal=pid.update(setpoint,plant_state)
        #物理模型更新(一阶惯性)
        plant_state+=(control_signal-plant_state)*(pid.dt/tau)
        #记录数据
        outputs.append(plant_state)
        controls.append(control_signal)
    #绘图
    plt.figure(figsize=(12,6))
    plt.plot(timestamps,outputs,label="System Output",linewidth=2)
    plt.plot(timestamps,controls,linestyle='--',label="Control Signal",linewidth=2)
    plt.axhline(y=setpoint,color='r',linestyle=':',label="Setpoint")
    plt.xlabel("Time(s)")
    plt.ylabel("Output")
    plt.title("Incremental PID Simulation")
    plt.grid(True,alpha=0.3)
    plt.legend()
    plt.savefig("pid_incremental_result.png",dpi=150)
    plt.show()
    print("仿真结束,查看生成的pid_incremental_result.png")
