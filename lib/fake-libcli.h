#ifndef __LIBCLI_H__
#define __LIBCLI_H__

#define CLI_OK			0
#define CLI_ERROR		-1
#define CLI_QUIT		-2
#define MAX_HISTORY		256

#define PRIVILEGE_UNPRIVILEGED	0
#define PRIVILEGE_PRIVILEGED	15
#define MODE_ANY		-1
#define MODE_EXEC		0
#define MODE_CONFIG		1

#include <stdio.h>

struct cli_def
{
    int completion_callback;
    struct cli_command *commands;
    int (*auth_callback)(char *, char *);
    int (*regular_callback)(struct cli_def *cli);
    int (*enable_callback)(char *);
    char *banner;
    struct unp *users;
    char *enable_password;
    char *history[MAX_HISTORY];
    char showprompt;
    char *promptchar;
    char *hostname;
    char *modestring;
    int privilege;
    int mode;
    int state;
    struct cli_filter *filters;
    void (*print_callback)(struct cli_def *cli, char *string);
    FILE *client;
};

struct cli_filter
{
    int (*filter)(struct cli_def *cli, char *string, void *data);
    void *data;
    struct cli_filter *next;
};

struct cli_command
{
    char *command;
    int (*callback)(struct cli_def *, char *, char **, int);
    int unique_len;
    char *help;
    int privilege;
    int mode;
    struct cli_command *next;
    struct cli_command *children;
    struct cli_command *parent;
};

struct cli_def *cli_init();
void cli_print(struct cli_def *cli, char *format, ...) __attribute__((format (printf, 2, 3)));
void cli_error(struct cli_def *cli, char *format, ...) __attribute__((format (printf, 2, 3)));
#endif
