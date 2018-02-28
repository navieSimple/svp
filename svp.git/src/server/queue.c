#include "svp.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "queuedef.h"

struct QUEUE_NAME *QUEUE_METHOD(create)()
{
    struct QUEUE_NAME *cmdq = (struct QUEUE_NAME *)malloc(sizeof(*cmdq));
    memset(cmdq, 0, sizeof(*cmdq));
    pthread_spin_init(&cmdq->lock, PTHREAD_PROCESS_PRIVATE);
    return cmdq;
}

void QUEUE_METHOD(destroy)(struct QUEUE_NAME *cmdq)
{
    if (!cmdq)
        return;
    pthread_spin_destroy(&cmdq->lock);
    free(cmdq);
}

int QUEUE_METHOD(push)(struct QUEUE_NAME *cmdq, int cmd, void *data, int size)
{
    int i;

    pthread_spin_lock(&cmdq->lock);
    if (cmdq->tail - cmdq->head >= SVP_CMDQUEUE_SIZE) {
        pthread_spin_unlock(&cmdq->lock);
        dzlog_debug("cmd queue full");
        return -1;
    }
    i = cmdq->tail & SVP_CMDQUEUE_MASK;
    cmdq->cmd[i].type = cmd;
    cmdq->cmd[i].data = (uint8_t *)data;
    cmdq->cmd[i].size = size;
    cmdq->tail++;
    pthread_spin_unlock(&cmdq->lock);
    return 0;
}

int QUEUE_METHOD(peek)(struct QUEUE_NAME *cmdq, struct QUEUE_ITEM *cmd)
{
    int i;

    pthread_spin_lock(&cmdq->lock);
    if (cmdq->head >= cmdq->tail) {
        pthread_spin_unlock(&cmdq->lock);
        return -1;
    }
    i = cmdq->head & SVP_CMDQUEUE_MASK;
    memcpy(cmd, &cmdq->cmd[i], sizeof(*cmd));
    pthread_spin_unlock(&cmdq->lock);
    return 0;
}

int QUEUE_METHOD(pop)(struct QUEUE_NAME *cmdq, struct QUEUE_ITEM *cmd)
{
    int i;

    pthread_spin_lock(&cmdq->lock);
    if (cmdq->head >= cmdq->tail) {
        pthread_spin_unlock(&cmdq->lock);
        return 0;
    }
    i = cmdq->head & SVP_CMDQUEUE_MASK;
    memcpy(cmd, &cmdq->cmd[i], sizeof(*cmd));
    cmdq->head++;
    pthread_spin_unlock(&cmdq->lock);
    return 1;
}

int QUEUE_METHOD(popn)(struct QUEUE_NAME *cmdq, struct QUEUE_ITEM *cmd, int max_num)
{
    int idx;
    int i;
    int j;
    int num;

    if (max_num <= 0)
        return 0;

    pthread_spin_lock(&cmdq->lock);
    if (cmdq->head >= cmdq->tail) {
        pthread_spin_unlock(&cmdq->lock);
        return 0;
    }
    num = cmdq->tail - cmdq->head;
    if (num > max_num)
        num = max_num;
    for (i = 0; i < num; i++) {
        idx = (cmdq->head + i) & SVP_CMDQUEUE_MASK;
        memcpy(&cmd[i], &cmdq->cmd[idx], sizeof(*cmd));
    }
    cmdq->head += num;
    pthread_spin_unlock(&cmdq->lock);
    return num;
}

int QUEUE_METHOD(size)(struct QUEUE_NAME *cmdq)
{
    int num;

    pthread_spin_lock(&cmdq->lock);
    num = cmdq->tail - cmdq->head;
    pthread_spin_unlock(&cmdq->lock);

    return num;
}

void QUEUE_METHOD(clear)(struct QUEUE_NAME *cmdq, QUEUE_CLEANUP_FUNCTION cleanup)
{
    int idx;
    int i;
    int num;

    pthread_spin_lock(&cmdq->lock);
    num = cmdq->tail - cmdq->head;
    for (i = 0; i < num; i++) {
        idx = (cmdq->head + i) & SVP_CMDQUEUE_MASK;
        if (cleanup)
            cleanup(&cmdq->cmd[idx]);
    }
    cmdq->head = cmdq->tail = 0;
    pthread_spin_unlock(&cmdq->lock);
}
