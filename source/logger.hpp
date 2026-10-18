#ifndef __M_LOGGER_H__
#define __M_LOGGER_H__
#include <stdio.h>
#include <time.h>

#define INF 0
#define DBG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL INF
#define LOG(level, format, ...) do{\
    if (DEFAULT_LOG_LEVEL > level) break;\
    time_t t = time(NULL);\
    struct tm *lt = localtime(&t);\
    char buf[32] = {0};\
    strftime(buf, 31, "%H:%M:%S", lt);\
    fprintf(stdout, "[%s %s:%d] " format "\n", buf, __FILE__, __LINE__, ##__VA_ARGS__);\
}while(0)
#define ILOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DLOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)

#endif