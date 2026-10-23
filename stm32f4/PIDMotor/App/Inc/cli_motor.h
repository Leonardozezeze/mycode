#ifndef __CLI_MOTOR
#define __CLI_MOTOR

#ifdef __cplusplus
#include "shell.h"
extern "C" {
#endif

// 注册 motor 命令到指定的 Shell 实例
void motor_cli_register(Shell* shell);

#ifdef __cplusplus
}
#endif

#endif
