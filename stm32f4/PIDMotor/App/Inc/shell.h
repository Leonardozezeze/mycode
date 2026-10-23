/* shell.h - C++ Shell class */
#ifndef SHELL_H
#define SHELL_H

#include "ringbuf.h"

#ifdef __cplusplus

class Shell {
public:
    typedef int (*cmd_fn)(int argc, char **argv);

    struct shell_cmd_t {
        const char *name;
        cmd_fn fn;
        const char *help;
    };

private:
    static const int SHELL_MAX_CMDS = 32;
    static const int LINE_MAX = 128;
    static const int ARG_MAX = 16;
    static const int SHELL_HISTORY_N = 5;

    enum esc_state_t {
        ESC_NORMAL = 0,
        ESC_GOT_ESC,
        ESC_GOT_BRACKET
    };

    // 实例状态
    shell_cmd_t cmds[SHELL_MAX_CMDS];
    int ncmds;

    char line[LINE_MAX];
    int len;

    char hist[SHELL_HISTORY_N][LINE_MAX];
    int hist_count;
    int hist_idx;
    char draft[LINE_MAX];
    int draft_len;

    esc_state_t esc_state;

    const char* prompt;
    void (*output_fn)(const char*);
    RingBuffer* rx_buf; // 输入环形缓冲区

    // 内部方法
    void exec_line(char *s);
    void history_push(const char *cmd);
    void line_replace(const char *s);
    void history_prev(void);
    void history_next(void);
    void puts(const char *s);

public:
    // 构造函数
    Shell(const char* prompt_str, void (*output)(const char*), RingBuffer* rx_buffer);

    // 注册命令(供 CLI 模块调用)
    int register_cmd(const char *name, cmd_fn fn, const char *help);

    // 处理输入(从环形缓冲区读取一个字符并处理)
    void process_input(void);

    // 直接喂字符(兼容旧接口)
    void feed(char c);

    // 获取输出函数(供 CLI 模块使用)
    void (*get_output_fn(void))(const char*) { return output_fn; }

    // 初始化
    void init(void);
};

#endif // __cplusplus

#endif // SHELL_H
