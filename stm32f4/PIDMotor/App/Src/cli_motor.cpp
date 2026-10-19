#include "cli_motor.h"
#include "shell.h"
#include "motor.h"
#include "control.h" /* extern PID motor_pid + Control_GetPWM/SetPWM/Lock/Unlock */
#include "encoder.h" /* Encoder_GetSpeedRPM()(中断更新的缓存值) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 辅助：解析整数 */
static int parse_int(const char *s, long *out)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (*end || end == s)
        return -1;
    *out = v;
    return 0;
}

/* motor pwm <value> — 手动直驱(PID 开启时禁止,否则 10ms 内被控制中断覆盖) */
static int motor_pwm(int argc, char **argv)
{
    if (argc < 3) {
        shell_puts("usage: motor pwm <value>\r\n");
        return -1;
    }
    long v;
    if (parse_int(argv[2], &v) < 0) {
        shell_puts("bad number\r\n");
        return -1;
    }
    if (v < -MOTOR_PWM_MAX || v > MOTOR_PWM_MAX) {
        shell_puts("out of range\r\n");
        return -1;
    }
    if (motor_pid.getStatus()) {
        shell_puts("pid is on, use: motor pid off\r\n");
        return -1;
    }
    Control_SetPWM((int16_t)v); /* 同时更新缓存,motor status 显示真实占空比 */
    shell_puts("ok\r\n");
    return 0;
}


/* motor status — 实际 PWM + 实测转速 + PID 状态
 * 各字段是对 ISR 更新变量的独立原子读(可能跨相邻 10ms 拍),显示足够 */
static int motor_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("PWM: %d/%d  Speed: %.1f RPM  PID: %s\r\n",
           (int)Control_GetPWM(), MOTOR_PWM_MAX,
           Encoder_GetSpeedRPM(),
           motor_pid.getStatus() ? "on" : "off");
    return 0;
}

/* motor enc — 编码器测速内部状态(调试用) */
static int motor_enc(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    Encoder_DebugDump();
    return 0;
}

/* 辅助：解析浮点数 */
static int parse_float(const char *s, float *out)
{
    char *end;
    float v = strtof(s, &end);
    if (*end || end == s)
        return -1;
    *out = v;
    return 0;
}

/* motor pid — PID 参数配置与状态控制 */
static int motor_pid_status(int argc, char **argv)
{
    if (argc < 3)
    {
        shell_puts("usage: motor pid <status/on/off/set/speed> [args...]\r\n");
        return -1;
    }

    if(strcmp(argv[2],"status")==0)
    {
        printf("PID: %s\r\n", motor_pid.getStatus() ? "ON" : "OFF");
        printf("Kp: %.4f  Ki: %.4f  Kd: %.4f\r\n",
               motor_pid.getKp(), motor_pid.getKi(), motor_pid.getKd());
        printf("Target: %.2f RPM  Actual: %.2f RPM\r\n",
               motor_pid.getTarget(), motor_pid.getActual());
        printf("Output: %.2f  PWM: %d/%d\r\n",
               motor_pid.getOutput(), (int)Control_GetPWM(), MOTOR_PWM_MAX);
    }
    else if (strcmp(argv[2], "on")==0)
    {
        motor_pid.enable(); /* 单字节存储,原子,无需临界区 */
        shell_puts("PID enabled\r\n");
    }
    else if (strcmp(argv[2], "off") == 0)
    {
        /* disable+reset 是多字段序列,屏蔽控制中断防止 ISR 读到撕裂状态 */
        Control_Lock();
        motor_pid.disable();
        motor_pid.reset();
        Control_Unlock();
        shell_puts("PID disabled\r\n");
    }
    else if (strcmp(argv[2], "set") == 0)
    {
        if (argc < 6)
        {
            shell_puts("usage: motor pid set <kp> <ki> <kd>\r\n");
            return -1;
        }

        float kp, ki, kd;
        if (parse_float(argv[3], &kp) < 0 ||
            parse_float(argv[4], &ki) < 0 ||
            parse_float(argv[5], &kd) < 0)
        {
            shell_puts("bad number\r\n");
            return -1;
        }

        Control_Lock();  // 屏蔽中断,防止 ISR 读到中间状态
        motor_pid.setPID(kp, ki, kd);
        motor_pid.reset();  // 切换参数后清零历史,避免扰动
        Control_Unlock();

        printf("PID updated: Kp=%.4f Ki=%.4f Kd=%.4f\r\n", kp, ki, kd);
    }
    else if (strcmp(argv[2], "speed") == 0)
    {
        if (argc < 4)
        {
            shell_puts("usage: motor pid speed <rpm>\r\n");
            return -1;
        }

        long rpm;
        if (parse_int(argv[3], &rpm) < 0) {
            shell_puts("bad number\r\n");
            return -1;
        }

        motor_pid.setTarget((float)rpm); /* 单个对齐 float 存储,CM4 上原子,无需临界区 */
        shell_puts("ok\r\n");
    }
    else{
        shell_puts("unknown subcommand\r\n");
        return -1;
    }
    return 0;
}

/* 命令函数 */
static int cmd_motor(int argc, char **argv)
{
    if (argc < 2) {
        shell_puts("usage: motor <pwm|status|enc|pid> [args...]\r\n");
        return -1;
    }

    if (strcmp(argv[1], "pwm") == 0)
        return motor_pwm(argc, argv);
    if (strcmp(argv[1], "status") == 0)
        return motor_status(argc, argv);
    if (strcmp(argv[1], "enc") == 0)
        return motor_enc(argc, argv);
    if (strcmp(argv[1], "pid") == 0)
        return motor_pid_status(argc, argv);

    shell_puts("unknown subcommand\r\n");
    return -1;
}

/* 注册函数 */
void motor_cli_register(void)
{
    shell_register("motor", cmd_motor, "motor <pwm|status|enc|pid>  (pid: status|on|off|set <kp> <ki> <kd>|speed <rpm>)");
}
