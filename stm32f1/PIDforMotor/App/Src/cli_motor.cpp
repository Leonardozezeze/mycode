#include "cli_motor.h"
#include "shell.h"
#include "motor.h"
#include "pid.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern PID motor_pid;

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

/* motor pwm <value> */
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
    Motor_Set((uint16_t)v);
    shell_puts("ok\r\n");
    return 0;
}

/* motor speed <rpm> */
static int motor_speed(int argc, char **argv)
{
    if (argc < 3) {
        shell_puts("usage: motor speed <rpm>\r\n");
        return -1;
    }
    long rpm;
    if (parse_int(argv[2], &rpm) < 0) {
        shell_puts("bad number\r\n");
        return -1;
    }
    motor_pid.setTarget((float)rpm);
    shell_puts("ok\r\n");
    return 0;
}

/* motor pid — 显示 PID 当前状态 */
static int motor_pid_status(int argc, char **argv)
{
    printf("target: %.2f\r\n", motor_pid.getTarget());
    printf("actual: %.2f\r\n", motor_pid.getActual());
    printf("output: %.2f\r\n", motor_pid.getOutput());
    return 0;
}

/* 命令函数 */
static int cmd_motor(int argc, char **argv)
{
    if (argc < 2) {
        shell_puts("usage: motor <pwm|speed|pid> [args...]\r\n");
        return -1;
    }

    if (strcmp(argv[1], "pwm") == 0)
        return motor_pwm(argc, argv);
    if (strcmp(argv[1], "speed") == 0)
        return motor_speed(argc, argv);
    if (strcmp(argv[1], "pid") == 0)
        return motor_pid_status(argc, argv);

    shell_puts("unknown subcommand\r\n");
    return -1;
}

/* 注册函数 */
void motor_cli_register(void)
{
    shell_register("motor", cmd_motor, "motor <pwm|speed|pid> [args]");
}
