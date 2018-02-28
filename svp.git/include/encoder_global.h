#ifndef ENCODER_GLOBAL_H
#define ENCODER_GLOBAL_H

#define ENCODER_PUBLIC __attribute__((visibility("default")))
#define ENCODER_CREATE_SYMBOL "encoder_plugin_create_encoder"
typedef struct encoder_base *(*encoder_create_fn)();

#endif // ENCODER_GLOBAL_H
