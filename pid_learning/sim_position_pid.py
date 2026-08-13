import matplotlib.pyplot as plt

class PositionPID:
    def __init__(self,Kp,Ki,Kd,dt,
                 output_max=100.0,output_min=-100.0,
                 integral_max=100.0,integral_min=-100.0):
        #控制增益
        self.Kp=Kp
        self.Ki=Ki
        self.Kd=Kd
        self.dt=dt
        #(执行边界)输出限幅
        self.output_max=output_max
        self.output_min=output_min
        #积分限幅
        self.integral_max=integral_max
        self.integral_min=integral_min
        #内部状态变量
        self._integral=0.0#积分累加值
        self._prev_error=0.0#上一次误差
        self._prev_output=0.0#上一次输出

        self.first_run=True

    def reset(self):
        self._integral=0.0
        self._prev_error=0.0
        self._prev_output=0.0
        self.first_run=True
        print("PID内部状态已重置")

    def update(self,setpoint,measurement):
        error=setpoint-measurement
        self._integral+=error*self.dt#积分项
        #积分限幅
        if self._integral>self.integral_max:
            self._integral=self.integral_max
        elif self._integral<self.integral_min:
            self._integral=self.integral_min
        #微分项
        if self.first_run:
            derivative=0.0
            self.first_run=False
        else:
            derivative=(error-self._prev_error)/self.dt

        raw_output=(self.Kp*error)+(self.Ki*self._integral)+(self.Kd*derivative)
        #输出限幅+积分退绕
        if raw_output>self.output_max:
            control_signal=self.output_max
            if self.Ki!=0:
                clamped_integral=(self.output_max-self.Kp*error-self.Kd*derivative)/self.Ki
                if clamped_integral<self._integral:
                    self._integral=clamped_integral
                if self._integral>self.integral_max:
                    self._integral=self.integral_max
                elif self._integral<self.integral_min:
                    self._integral=self.integral_min
        elif raw_output<self.output_min:
            control_signal=self.output_min
            if self.Ki!=0:
                clamped_integral=(self.output_min-self.Kp*error-self.Kd*derivative)/self.Ki
                if clamped_integral>self._integral:
                    self._integral=clamped_integral
                if self._integral>self.integral_max:
                    self._integral=self.integral_max
                elif self._integral<self.integral_min:
                    self._integral=self.integral_min
        else:
            control_signal=raw_output
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
    pid=PositionPID(
        Kp=0.8,
        Ki=0.4,
        Kd=0.05,
        dt=0.01,
        output_max=100.0,
        output_min=-100.0,
        integral_max=50.0,
        integral_min=-50.0
    )
    #系统变量
    setpoint=20.0#目标值
    time=20#仿真时间
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
    plt.title("Position PID with Anti-windup Simulation")
    plt.grid(True,alpha=0.3)
    plt.legend()
    plt.savefig("pid_template_result.png",dpi=150)
    plt.show()
    print("仿真结束,查看生成的pid_template_result.png")

