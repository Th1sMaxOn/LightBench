#include "protocol.h"
#include <string.h>

void protocol_overflow(CommandReader *reader) {
    reader->length = 0;
    reader->dropping = true;
}
BenchCommand protocol_feed(CommandReader *reader, int ch) {
    if (ch == '\r' || ch == '\n') {
        BenchCommand result = CMD_NONE;
        if (reader->dropping) result = CMD_BAD_LINE;
        else if (reader->length) {
            reader->line[reader->length] = 0;
            const char *names[] = {"run", "stop", "status", "export", "selftest", "imuaddr 0x68", "imuaddr 0x69",
                "metrics", "metrics reset", "period 20", "period 10", "period 5", "period 2", "period 1"};
            result = CMD_UNKNOWN;
            for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
                if (!strcmp(reader->line, names[i])) { result = (BenchCommand)(CMD_RUN + i); break; }
        }
        reader->length = 0;
        reader->dropping = false;
        return result;
    }
    if (reader->dropping) return CMD_NONE;
    if (ch < 32 || ch > 126 || reader->length == sizeof(reader->line) - 1u)
        protocol_overflow(reader);
    else reader->line[reader->length++] = (char)ch;
    return CMD_NONE;
}
