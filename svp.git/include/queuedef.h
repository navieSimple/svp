#ifndef QUEUE_PREFIX
#error "you must define QUEUE_PREFIX before include queue.h"
#endif

#define X_PREFIX QUEUE_PREFIX
#define CONCAT(a,b) a##b
#define X_CONCAT(a,b) CONCAT(a,b)
#define QUEUE_NAME X_CONCAT(X_PREFIX, queue)
#define QUEUE_METHOD(x) X_CONCAT(X_CONCAT(X_PREFIX, q_), x)
#define QUEUE_ITEM X_CONCAT(X_PREFIX, _cmd)

#define QUEUE_CLEANUP_FUNCTION X_CONCAT(X_PREFIX, _cleanup_function)
