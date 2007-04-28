#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#ifdef __linux__
#include <malloc.h>
#endif
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "libcli.h"
// vim:sw=8 ts=8

struct cli_def *cli_init()
{
	struct cli_def *cli;
//	struct cli_command *c;

	if (!(cli = calloc(sizeof(struct cli_def), 1)))
		return NULL;

	return cli;
}

void cli_print(struct cli_def *cli, char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	(void)vfprintf(stdout, format, ap);
	va_end(ap);
        fprintf(stdout,"\n");
}

void cli_error(struct cli_def *cli, char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);
        fprintf(stderr,"\n");
}

