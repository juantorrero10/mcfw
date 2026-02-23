#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>

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

#define log_fatal(msg, ...) fprintf(stderr, COLOR_RED"" STYLE_BOLD "fatal: " COLOR_RESET "" COLOR_WHITE "" msg "" COLOR_RESET "\033[K\r\n", ##__VA_ARGS__)
#define log_okay(msg, ...) fprintf(stdout, COLOR_BRIGHT_GREEN"" STYLE_BOLD "done: " COLOR_RESET "" msg "\033[K\r\n", ##__VA_ARGS__)
#define log_warn(msg, ...) fprintf(stderr, COLOR_YELLOW"warn: " COLOR_RESET "" msg "\r\n", ##__VA_ARGS__)
#define log_inprogress(operation, msg, ...) fprintf(stdout, COLOR_CYAN"" STYLE_BOLD "%s: " COLOR_RESET "" msg "\033[K\r",operation,  ##__VA_ARGS__); fflush(stdout)
#define log_running(msg, ...) fprintf(stdout, COLOR_BRIGHT_BLUE""STYLE_BOLD"running: "COLOR_RESET""COLOR_WHITE""msg""COLOR_RESET"\r\n", ##__VA_ARGS__)

#endif // LOG_H_