#pragma once
#include <stdarg.h>

void logcmd(char* fmt, ...);

int init_logfile(char* filename);

int logfile(char* file, int line, char* msg);
