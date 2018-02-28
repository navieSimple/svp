#include "queuedef.h"
#include <pthread.h>

#include "svp.h"

typedef void (*QUEUE_CLEANUP_FUNCTION)(struct QUEUE_ITEM *);

struct QUEUE_NAME
{
    pthread_spinlock_t lock;
    int head;
    int tail;
    struct QUEUE_ITEM cmd[SVP_CMDQUEUE_SIZE];
};

struct QUEUE_NAME *QUEUE_METHOD(create)();

void QUEUE_METHOD(destroy)(struct QUEUE_NAME *cmdq);

int QUEUE_METHOD(push)(struct QUEUE_NAME *cmdq, int cmd, void *data, int size);

int QUEUE_METHOD(peek)(struct QUEUE_NAME *cmdq, struct QUEUE_ITEM *cmd);

int QUEUE_METHOD(pop)(struct QUEUE_NAME *cmdq, struct QUEUE_ITEM *cmd);

int QUEUE_METHOD(popn)(struct QUEUE_NAME *cmdq, struct QUEUE_ITEM *cmd, int max_num);

int QUEUE_METHOD(size)(struct QUEUE_NAME *cmdq);

void QUEUE_METHOD(clear)(struct QUEUE_NAME *cmdq, QUEUE_CLEANUP_FUNCTION cleanup);
