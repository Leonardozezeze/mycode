/* shell.cpp - C++ Shell class implementation */
#include "shell.h"
#include <string.h>
#include <stdio.h>

// 构造函数
Shell::Shell(const char* prompt_str, void (*output)(const char*), RingBuffer* rx_buffer)
    : prompt(prompt_str), output_fn(output), rx_buf(rx_buffer)
{
    init();
}

// 初始化
void Shell::init(void)
{
    ncmds = 0;
    len = 0;
    hist_count = 0;
    hist_idx = -1;
    draft_len = 0;
    esc_state = ESC_NORMAL;
    puts(prompt);
}

// 注册命令
int Shell::register_cmd(const char *name, cmd_fn fn, const char *help)
{
    if (!name || !fn)
        return -1;
    if (ncmds >= SHELL_MAX_CMDS)
        return -1;

    // 查重
    for (int i = 0; i < ncmds; i++) {
        if (strcmp(cmds[i].name, name) == 0)
            return -1;
    }

    cmds[ncmds].name = name;
    cmds[ncmds].fn = fn;
    cmds[ncmds].help = help;
    ncmds++;
    return 0;
}

// 执行命令行
void Shell::exec_line(char *s)
{
    char *argv[ARG_MAX];
    int argc = 0;
    char *p = s;

    while (*p && argc < ARG_MAX) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }
    if (argc == 0)
        return;

    for (int i = 0; i < ncmds; i++) {
        if (strcmp(argv[0], cmds[i].name) == 0) {
            cmds[i].fn(argc, argv);
            return;
        }
    }
    puts("unknown command\r\n");
}

// 内部 puts 方法,使用实例的 output_fn
void Shell::puts(const char *s)
{
    if (output_fn) {
        output_fn(s);
    }
}

// 历史命令入栈
void Shell::history_push(const char *cmd)
{
    if (hist_count > 0 && strcmp(hist[hist_count - 1], cmd) == 0)
        return;
    if (hist_count == SHELL_HISTORY_N) {
        memmove(hist[0], hist[1], (size_t)(SHELL_HISTORY_N - 1) * LINE_MAX);
        hist_count--;
    }
    strncpy(hist[hist_count], cmd, LINE_MAX - 1);
    hist[hist_count][LINE_MAX - 1] = '\0';
    hist_count++;
}

// 替换当前编辑行
void Shell::line_replace(const char *s)
{
    while (len > 0) {
        puts("\b \b");
        len--;
    }
    len = (int)strlen(s);
    memcpy(line, s, (size_t)len);
    line[len] = '\0';
    puts(line);
}

// 上一条历史命令
void Shell::history_prev(void)
{
    if (hist_count == 0)
        return;

    if (hist_idx == -1) {
        memcpy(draft, line, (size_t)len);
        draft[len] = '\0';
        draft_len = len;
        hist_idx = hist_count - 1;
    }
    else if (hist_idx > 0) {
        hist_idx--;
    }
    else {
        return; // 已在最旧一条
    }
    line_replace(hist[hist_idx]);
}

// 下一条历史命令
void Shell::history_next(void)
{
    if (hist_idx == -1)
        return; // 不在浏览状态

    if (hist_idx < hist_count - 1) {
        hist_idx++;
        line_replace(hist[hist_idx]);
    }
    else {
        hist_idx = -1;
        draft[draft_len] = '\0';
        line_replace(draft);
    }
}

// 处理输入(从环形缓冲区读取一个字符并处理)
void Shell::process_input(void)
{
    if (!rx_buf)
        return;

    int c = rx_buf->pop();
    if (c >= 0) {
        feed((char)c);
    }
}

// 喂字符
void Shell::feed(char c)
{
    // ESC 转义序列状态机
    switch (esc_state) {
    case ESC_GOT_ESC:
        esc_state = ESC_NORMAL;
        if (c == '[') {
            esc_state = ESC_GOT_BRACKET;
            return;
        }
        break;
    case ESC_GOT_BRACKET:
        esc_state = ESC_NORMAL;
        if (c == 'A')
            history_prev();
        else if (c == 'B')
            history_next();
        return;
    case ESC_NORMAL:
    default:
        if (c == 0x1B) {
            esc_state = ESC_GOT_ESC;
            return;
        }
        break;
    }

    if (c == '\r' || c == '\n') {
        puts("\r\n");
        line[len] = '\0';
        if (len) {
            history_push(line);
            exec_line(line);
        }
        len = 0;
        hist_idx = -1;
        puts(prompt);
        return;
    }
    if (c == 0x7f || c == 0x08) { // 退格
        if (len > 0) {
            len--;
            puts("\b \b");
        }
        return;
    }
    if (c >= 0x20 && c < 0x7f && len < LINE_MAX - 1) {
        line[len++] = c;
        char b[2] = {c, 0};
        puts(b); // 回显
    }
}
