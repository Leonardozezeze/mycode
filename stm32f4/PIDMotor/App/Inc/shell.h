/* shell.h */
#ifndef SHELL_H
#define SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*cmd_fn)(int argc, char **argv);

void shell_init(void);
int shell_register(const char *name, cmd_fn fn, const char *help);
void shell_feed(char c);
void shell_puts(const char *s);

#ifdef __cplusplus
}
#endif

#endif