#include "log.h"

uint8_t logging_level = 2;

time_t t;      /* Declared here not to be allocated every call or sth */
struct tm *ct; /* Idk */
char time_string[64];

void set_logging_level(uint8_t level) {
  logging_level = level;
}

void print_begining(uint8_t severity, FILE *__restrict log_file) {
  t = time(NULL);
  ct = localtime(&t);
  
  strftime(time_string, sizeof(time_string), "%c", ct);
  fprintf(log_file, "[%s] %s %2u: ", time_string, R2K_LOG_NAME, severity);
}

void log_string(uint8_t severity, char *str, FILE *__restrict log_file) {
  if (severity >= logging_level) {
    print_begining(severity, log_file);
    fputs(str, log_file);
  }
}

void log_format(uint8_t severity, FILE *__restrict log_file, const char *format, ...) {
  if (severity >= logging_level) {
    print_begining(severity, log_file);
    
    va_list vars;
    va_start(vars, format);
    vfprintf(log_file, format, vars);
    va_end(vars);
  }
}
