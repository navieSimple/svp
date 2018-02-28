#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef BUILD_SVP_SERVER
#define GLOBAL_SETTINGS_PATH "/etc/svp.ini"
#else
#define GLOBAL_SETTINGS_PATH "svp.ini"
#endif
#define GS settings_global()

#define KEY_PREFER_CODEC    "General.PreferCodec"
#define KEY_LZ4_HC          "LZ4.HighCompression"

#ifdef __cplusplus
extern "C" {
#endif
struct settings;

struct settings *settings_global();

void settings_read_int(struct settings *, const char *prop, int *value, int def);

void settings_write_int(struct settings *, const char *prop, int value);

void settings_read_str(struct settings *, const char *prop, char *str, int max_size, char *def);

void settings_write_str(struct settings *, const char *prop, const char *str);

void settings_save(struct settings *, const char *fname);

void settings_restore(struct settings *, const char *fname);

void settings_update(struct settings *, const char *buf, int size);

int settings_dump(struct settings *, char *buf, int max_size);

void settings_clear(struct settings *);

void settings_remove(struct settings *, const char *prop);

int settings_exist(struct settings *, const char *prop);

#ifdef __cplusplus
}
#endif
#endif // SETTINGS_H
