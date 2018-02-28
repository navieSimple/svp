#include "settings.h"
#include "iniparser.h"
#ifdef BUILD_SVP_SERVER
#include "svp.h"
#else
#define dzlog_debug(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct settings
{
    pthread_mutex_t mutex;
    dictionary *dict;
};

static struct settings *g_settings = 0;
struct settings *settings_global()
{
    struct stat st;
    if (!g_settings) {
        g_settings = (struct settings *)malloc(sizeof(*g_settings));
        if (!g_settings)
            return 0;
        memset(g_settings, 0, sizeof(*g_settings));
        pthread_mutex_init(&g_settings->mutex, 0);
        if (stat(GLOBAL_SETTINGS_PATH, &st))
            creat(GLOBAL_SETTINGS_PATH, 0644);
        settings_restore(g_settings, GLOBAL_SETTINGS_PATH);
        if (!g_settings->dict) {
            dzlog_debug("load setting file %s error", GLOBAL_SETTINGS_PATH);
            exit(-1);
        }
    }
    return g_settings;
}

static const char *make_key(const char *orig)
{
    static char key[1024];
    if (strchr(orig, '.')) {
        strncpy(key, orig, sizeof(key) - 1);
        key[sizeof(key) - 1] = 0;
    } else {
        snprintf(key, sizeof(key), "__global.%s", orig);
    }
    return key;
}

int do_set(dictionary *ini, const char *key, const char *val)
{
    static char prefix[1024];
    int plen;
    char *p = strchr(key, '.');
    if (!p)
        return -1;
    plen = p - key;
    memcpy(prefix, key, plen);
    prefix[plen] = 0;
    iniparser_set(ini, prefix, 0);
    return iniparser_set(ini, key, val);
}

void settings_read_int(struct settings *s, const char *prop, int *value, int def)
{
    pthread_mutex_lock(&s->mutex);
    *value = iniparser_getint(s->dict, make_key(prop), def);
    pthread_mutex_unlock(&s->mutex);
}

void settings_write_int(struct settings *s, const char *prop, int value)
{
    char str[32];
    sprintf(str, "%d", value);
    pthread_mutex_lock(&s->mutex);
    do_set(s->dict, make_key(prop), str);
    pthread_mutex_unlock(&s->mutex);
}

void settings_read_str(struct settings *s, const char *prop, char *str, int max_size, char *def)
{
    char *p;
    pthread_mutex_lock(&s->mutex);
    p = iniparser_getstring(s->dict, make_key(prop), def);
    if (p) {
        strncpy(str, p, max_size - 1);
        str[max_size - 1] = 0;
    } else {
        strncpy(str, def, max_size - 1);
        str[max_size - 1] = 0;
    }
    pthread_mutex_unlock(&s->mutex);
}

void settings_write_str(struct settings *s, const char *prop, const char *str)
{
    pthread_mutex_lock(&s->mutex);
    do_set(s->dict, make_key(prop), str);
    pthread_mutex_unlock(&s->mutex);
}

void settings_update(struct settings *s, const char *buf, int size)
{
    dictionary *d;
    char fname[20] = ".update.ini";
    FILE *f;
    int nsec;
    int nkey;
    int i;
    int j;
    char *secname;
    char **keynames;
    char *val;

    f = fopen(fname, "wb");
    fwrite(buf, 1, size, f);
    fclose(f);
    d = iniparser_load(fname);
    if (!d)
        return;
    nsec = iniparser_getnsec(d);
    for (i = 0; i < nsec; i++) {
        secname = iniparser_getsecname(d, i);
        nkey = iniparser_getsecnkeys(d, secname);
        keynames = iniparser_getseckeys(d, secname);
        for (j = 0; j < nkey; j++) {
            val = iniparser_getstring(d, keynames[j], 0);
            do_set(s->dict, keynames[j], val);
        }
    }
}

int settings_dump(struct settings *s, char *buf, int max_size)
{
    char fname[20] = ".dump.ini";
    FILE *f;
    int n;
    if (!buf)
        return -1;
    settings_save(s, fname);
    f = fopen(fname, "rb");
    n = fread(buf, 1, max_size - 1, f);
    if (n >= 0) {
        buf[n] = 0;
        return 0;
    } else {
        *buf = 0;
        return -1;
    }
}

void settings_save(struct settings *s, const char *fname)
{
    FILE *f;
    pthread_mutex_lock(&s->mutex);
    if (!s->dict)
        goto out;
    f = fopen(fname, "w");
    if (!f)
        goto out;
    iniparser_dump_ini(s->dict, f);
    fclose(f);
out:
    pthread_mutex_unlock(&s->mutex);
}

void settings_restore(struct settings *s, const char *fname)
{
    pthread_mutex_lock(&s->mutex);
    if (s->dict)
        iniparser_freedict(s->dict);
    s->dict = iniparser_load(fname);
    pthread_mutex_unlock(&s->mutex);
}

void settings_clear(struct settings *s)
{
    pthread_mutex_lock(&s->mutex);
    iniparser_freedict(s->dict);
    s->dict = dictionary_new(32);
    pthread_mutex_unlock(&s->mutex);
}

void settings_remove(struct settings *s, const char *prop)
{
    pthread_mutex_lock(&s->mutex);
    iniparser_unset(s->dict, make_key(prop));
    pthread_mutex_unlock(&s->mutex);
}

int settings_exist(struct settings *s, const char *prop)
{
    int rc;
    pthread_mutex_lock(&s->mutex);
    rc = iniparser_find_entry(s->dict, make_key(prop));
    pthread_mutex_unlock(&s->mutex);
    return rc;
}
