#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define MAX_FORMAT_LEN 255

void printInfo(const char *format, ...);
void printError(const char *format, ...);