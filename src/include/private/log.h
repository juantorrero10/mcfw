#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include "configs.h"

void _get_time_str(char* buff);


#define COLOR_RESET          "\033[0m"
#define STYLE_BOLD          "\033[1m"
#define STYLE_ITALIC        "\033[3m"
#define STYLE_UNDERLINE     "\033[4m"

#define COLOR_BLACK       "\033[30m"
#define COLOR_RED         "\033[31m"
#define COLOR_GREEN       "\033[32m"
#define COLOR_YELLOW      "\033[33m"
#define COLOR_BLUE        "\033[34m"
#define COLOR_MAGENTA     "\033[35m"
#define COLOR_CYAN        "\033[36m"
#define COLOR_WHITE       "\033[37m"
#define COLOR_GREY       "\033[90m"


#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_WHITE   "\033[97m"

#define LOG_LEVEL_INFO 0
#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_ERROR 2

extern char g_time_buffer[];

#define mlog_info(msg, ...) do{\
    if (ARG_FLAGS.log_level > LOG_LEVEL_INFO) break;\
    _get_time_str(g_time_buffer);\
    printf("\033[Kmcfw: " COLOR_GREY"[%s]" COLOR_CYAN " INFO" COLOR_WHITE " (%s): "msg""COLOR_RESET"\r\n", g_time_buffer, __FUNCTION__, ##__VA_ARGS__);\
} while(0)

#define mlog_warn(msg, ...) do{\
    if (ARG_FLAGS.log_level > LOG_LEVEL_WARN) break;\
    _get_time_str(g_time_buffer);\
    printf("\033[Kmcfw: " COLOR_GREY"[%s]" COLOR_YELLOW " WARN" COLOR_WHITE " (%s): "msg""COLOR_RESET"\r\n", g_time_buffer, __FUNCTION__, ##__VA_ARGS__);\
} while(0)

#define mlog_error(msg, ...) do{\
    _get_time_str(g_time_buffer);\
    printf("\033[Kmcfw: " COLOR_GREY"[%s]" COLOR_RED " ERROR" COLOR_WHITE " (%s): "msg""COLOR_RESET"\r\n", g_time_buffer, __FUNCTION__, ##__VA_ARGS__);\
} while(0)

#define mlog_panic(msg, ...) do{\
    _get_time_str(g_time_buffer);\
    printf("\033[Kmcfw: " COLOR_GREY"[%s]" STYLE_BOLD"" COLOR_BRIGHT_RED " PANIC" COLOR_RESET " (%s): "msg""COLOR_RESET"\r\n", g_time_buffer, __FUNCTION__,##__VA_ARGS__);\
} while(0)


#endif // LOG_H_