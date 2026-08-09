import matplotlib.pyplot as plt

#参数
dt=0.01
t_end=10
setpoint=20
n_steps = int(t_end / dt)

Kp=0.5
Ki=0.678
Kd=0

#积分项和导数项
integral=0
derictive=0
res=0

#误差项
error=0
prev_error=0

timestamp=[]
value=[]

for i in range(n_steps):
    timestamp.append(dt*i)
    error=setpoint-res
    integral+=(error*dt)
    derictive=(error-prev_error)/dt
    res=Kp*error+Ki*integral+Kd*derictive
    value.append(res)
    prev_error=error

plt.figure(figsize=(10,6))
plt.plot(timestamp, value, label='System Output (PID Control)')  # 加上这个
plt.axhline(y=setpoint, color='r', linestyle='--', label='Setpoint (Target)')
plt.xlabel('Time (s)')
plt.ylabel('Output Value')
plt.title('PID Control of First-Order Inertia System (Hand-written Simulation)')
plt.grid(True)
plt.legend()
plt.savefig('pid_response.png', dpi=150)
