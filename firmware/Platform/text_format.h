#ifndef LIGHTBENCH_TEXT_FORMAT_H
#define LIGHTBENCH_TEXT_FORMAT_H
#include <stdarg.h>
#include <stddef.h>
int lb_vsnprintf(char *,size_t,const char *,va_list);
int lb_snprintf(char *,size_t,const char *,...);
#endif
