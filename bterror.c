#include "bterror.h"
#include "unistd.h"
#include <stdio.h>
#include <stdlib.h>

void btexit(int errno, char* file, int line)
{
    printf("exit at %s : %d with error number : %d\n", file, line, errno);
    exit(errno);
}