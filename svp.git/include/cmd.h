#ifndef CMD_H
#define CMD_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

struct in_cmd {
    int type;
    uint8_t *data;
    int size;
};

struct out_cmd {
    int type;
    uint8_t *data;
    int size;
};

void in_cmd_cleanup(struct in_cmd *cmd);

void out_cmd_cleanup(struct out_cmd *cmd);

void out_cmd_copy(struct out_cmd *dst, struct out_cmd *src);

#ifdef __cplusplus
}
#endif
#endif // CMD_H
