/* shell.c */
#include "shell.h"
#include <string.h>
#include <stdio.h>

#define SHELL_MAX_CMDS 32
#define LINE_MAX 128
#define ARG_MAX 16
#define PROMPT "> "

typedef struct
{
    const char *name;
    cmd_fn fn;
    const char *help;
} shell_cmd_t;

static shell_cmd_t g_cmds[SHELL_MAX_CMDS];
static int g_ncmds;

static char line[LINE_MAX];
static int len;

/* ---------- 注册 ---------- */
int shell_register(const char *name, cmd_fn fn, const char *help)
{
    if (!name || !fn)
        return -1;
    if (g_ncmds >= SHELL_MAX_CMDS)
        return -1;
    /* 查重，可选 */
    for (int i = 0; i < g_ncmds; i++)
        if (strcmp(g_cmds[i].name, name) == 0)
            return -1;

    g_cmds[g_ncmds].name = name;
    g_cmds[g_ncmds].fn = fn;
    g_cmds[g_ncmds].help = help;
    g_ncmds++;
    return 0;
}

/* ---------- 分发 ---------- */
static void exec_line(char *s)
{
    char *argv[ARG_MAX];
    int argc = 0;
    char *p = s;

    while (*p && argc < ARG_MAX)
    {
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

    for (int i = 0; i < g_ncmds; i++)
    {
        if (strcmp(argv[0], g_cmds[i].name) == 0)
        {
            g_cmds[i].fn(argc, argv); /* ← 关键：通过存下来的函数指针调用 */
            return;
        }
    }
    shell_puts("unknown command\r\n");
}

/* ---------- 输入 ---------- */
void shell_feed(char c)
{
    if (c == '\r' || c == '\n')
    {
        shell_puts("\r\n");
        line[len] = '\0';
        if (len)
            exec_line(line);
        len = 0;
        shell_puts(PROMPT);
        return;
    }
    if (c == 0x7f || c == 0x08)
    { /* 退格 */
        if (len > 0)
        {
            len--;
            shell_puts("\b \b");
        }
        return;
    }
    if (c >= 0x20 && c < 0x7f && len < LINE_MAX - 1)
    {
        line[len++] = c;
        char b[2] = {c, 0};
        shell_puts(b); /* 回显 */
    }
}

void shell_init(void)
{
    g_ncmds = 0;
    len = 0;
    setbuf(stdout, NULL); /* 禁用 stdout 缓冲，实现即时回显 */
    shell_puts(PROMPT);
}

void shell_puts(const char *s) { printf("%s", s); }