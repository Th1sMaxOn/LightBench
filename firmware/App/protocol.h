#ifndef LIGHTBENCH_PROTOCOL_H
#define LIGHTBENCH_PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    CMD_NONE, CMD_RUN, CMD_STOP, CMD_STATUS, CMD_EXPORT, CMD_SELFTEST,
    CMD_ADDR68, CMD_ADDR69, CMD_UNKNOWN, CMD_BAD_LINE
} BenchCommand;
typedef struct { char line[48]; unsigned length; bool dropping; } CommandReader;
BenchCommand protocol_feed(CommandReader *reader, int ch);
void protocol_overflow(CommandReader *reader);
#endif
