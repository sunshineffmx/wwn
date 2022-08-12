#include "debug.h"

void printInfo(const char *format, ...)
{
    char buffer[MAX_FORMAT_LEN];

    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    printf("%s\n", buffer);
}

void printError(const char *format, ...)
{
    char buffer[MAX_FORMAT_LEN];
    printf("Error: ");

    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    
    printf("%s\n", buffer);
    exit(1);
}