#include "log.h"
#include "unistd.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
FILE* logfile_fp = NULL;
void  logcmd(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}
int init_logfile(char* filename)
{
    logfile_fp = fopen(filename, "wb");
    if (logfile_fp < 0) {
        printf("open logfile failed\n");
        return -1;
    }
    return 0;
}
int logfile(char* file, int line, char* msg)
{
    char buff[256];
    if (logfile_fp < 0) return -1;
    snprintf(buff, 256, "%s:%d %s\n", file, line, msg);
    fwrite(buff, 1, strlen(buff), logfile_fp);
    return 0;
}