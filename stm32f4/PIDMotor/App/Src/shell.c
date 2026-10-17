/* shell.c */
#include "shell.h"
#include <string.h>
#include <stdio.h>

#define SHELL_MAX_CMDS 32
#define LINE_MAX 128
#define ARG_MAX 16
#define PROMPT "> "
#define SHELL_HISTORY_N 5 /* ↑/↓ 可回溯的历史命令条数 */

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

/* ---------- 命令历史(↑/↓ 回溯) ---------- */
static char s_hist[SHELL_HISTORY_N][LINE_MAX]; /* 从旧到新存放 */
static int s_hist_count = 0;                   /* 已存条数(≤ SHELL_HISTORY_N) */
static int s_hist_idx = -1;                    /* 浏览索引:-1=当前草稿行,否则指向 s_hist */
static char s_draft[LINE_MAX];                 /* 首次按 ↑ 时暂存的未执行草稿 */
static int s_draft_len = 0;

/* ESC 转义序列解析状态(方向键 = ESC '[' 'A'/'B',经 ringbuf 逐字节到达) */
typedef enum
{
    ESC_NORMAL = 0,
    ESC_GOT_ESC,     /* 已收到 0x1B */
    ESC_GOT_BRACKET  /* 已收到 0x1B '[' */
} esc_state_t;
static esc_state_t s_esc = ESC_NORMAL;

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

/* ---------- 历史操作 ---------- */

/* 入栈一条命令:与最近一条相同则跳过;满则挤掉最旧的一条 */
static void history_push(const char *cmd)
{
    if (s_hist_count > 0 && strcmp(s_hist[s_hist_count - 1], cmd) == 0)
        return;
    if (s_hist_count == SHELL_HISTORY_N)
    {
        memmove(s_hist[0], s_hist[1], (size_t)(SHELL_HISTORY_N - 1) * LINE_MAX);
        s_hist_count--;
    }
    strncpy(s_hist[s_hist_count], cmd, LINE_MAX - 1);
    s_hist[s_hist_count][LINE_MAX - 1] = '\0';
    s_hist_count++;
}

/* 用 s 整行替换当前编辑行并重绘。
 * 本 shell 是纯追加式编辑(光标恒在行尾),先逐字符擦除再回显即安全 */
static void line_replace(const char *s)
{
    while (len > 0)
    {
        shell_puts("\b \b");
        len--;
    }
    len = (int)strlen(s);
    memcpy(line, s, (size_t)len);
    line[len] = '\0';
    shell_puts(line);
}

/* ↑:向更旧浏览。首次离开当前行时暂存草稿;到最旧一条停住 */
static void history_prev(void)
{
    if (s_hist_count == 0)
        return;

    if (s_hist_idx == -1)
    {
        memcpy(s_draft, line, (size_t)len);
        s_draft[len] = '\0';
        s_draft_len = len;
        s_hist_idx = s_hist_count - 1; /* 最新一条 */
    }
    else if (s_hist_idx > 0)
    {
        s_hist_idx--;
    }
    else
    {
        return; /* 已在最旧一条 */
    }
    line_replace(s_hist[s_hist_idx]);
}

/* ↓:向更新浏览。越过最新一条后恢复草稿 */
static void history_next(void)
{
    if (s_hist_idx == -1)
        return; /* 不在浏览状态 */

    if (s_hist_idx < s_hist_count - 1)
    {
        s_hist_idx++;
        line_replace(s_hist[s_hist_idx]);
    }
    else
    {
        s_hist_idx = -1;
        s_draft[s_draft_len] = '\0';
        line_replace(s_draft);
    }
}

/* ---------- 输入 ---------- */
void shell_feed(char c)
{
    /* ESC 转义序列状态机(↑ = ESC [ A,↓ = ESC [ B) */
    switch (s_esc)
    {
    case ESC_GOT_ESC:
        s_esc = ESC_NORMAL;
        if (c == '[')
        {
            s_esc = ESC_GOT_BRACKET;
            return;
        }
        break; /* 孤立 ESC:丢弃转义态,本字节按普通字符继续处理 */
    case ESC_GOT_BRACKET:
        s_esc = ESC_NORMAL;
        if (c == 'A')
            history_prev();
        else if (c == 'B')
            history_next();
        /* 其余序列(←/→ 等)整段丢弃 */
        return;
    case ESC_NORMAL:
    default:
        if (c == 0x1B)
        {
            s_esc = ESC_GOT_ESC;
            return;
        }
        break;
    }

    if (c == '\r' || c == '\n')
    {
        shell_puts("\r\n");
        line[len] = '\0';
        if (len)
        {
            history_push(line); /* 先入栈再执行(exec_line 会就地改写 line) */
            exec_line(line);
        }
        len = 0;
        s_hist_idx = -1; /* 复位浏览状态 */
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
    s_hist_count = 0;
    s_hist_idx = -1;
    s_draft_len = 0;
    s_esc = ESC_NORMAL;
    setbuf(stdout, NULL); /* 禁用 stdout 缓冲，实现即时回显 */
    shell_puts(PROMPT);
}

void shell_puts(const char *s) { printf("%s", s); }