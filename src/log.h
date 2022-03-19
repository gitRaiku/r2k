#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdint.h>

#ifndef R2K_LOG_NAME
#define R2K_LOG_NAME "R2K"
#endif

void set_logging_level(uint8_t level);
void log_string(uint8_t severity, char *str, FILE *__restrict log_file);
void log_format(uint8_t severity, FILE *__restrict log_file, const char *format, ...);

#endif
