#include "encoder.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

struct encoder_base *encoder_load(const char *libpath)
{
    void *lib = dlopen(libpath, RTLD_NOW);
    if (!lib) {
        dzlog_debug("dlopen %s error:%s", libpath, dlerror());
        return 0;
    }
    encoder_create_fn create = (encoder_create_fn)dlsym(lib, ENCODER_CREATE_SYMBOL);
    if (!create) {
        dzlog_debug("%s dlsym error, not found entry '%s'", libpath, ENCODER_CREATE_SYMBOL);
        dlclose(lib);
        return 0;
    }
    return create();
}
