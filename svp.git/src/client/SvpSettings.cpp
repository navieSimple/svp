#include "SvpSettings.h"
#include "settings.h"
#include <QtGlobal>

Q_GLOBAL_STATIC(SvpSettings, global)

SvpSettings::SvpSettings()
{
    if (!settings_exist(GS, KEY_PREFER_CODEC))
        settings_write_int(GS, KEY_PREFER_CODEC, SX_RAW);
    if (!settings_exist(GS, KEY_LZ4_HC))
        settings_write_int(GS, KEY_LZ4_HC, 0);
}

SvpSettings::~SvpSettings()
{

}

SvpSettings *SvpSettings::instance()
{
    return global();
}

QByteArray SvpSettings::dump() const
{
    QByteArray b(4096, Qt::Uninitialized);
    int rc = settings_dump(GS, b.data(), b.size());
    if (rc)
        b.clear();
    else
        b.truncate(strlen(b.constData()));
    return b;
}

void SvpSettings::save()
{
    settings_save(GS, GLOBAL_SETTINGS_PATH);
}

int SvpSettings::preferCodec() const
{
    int codec;
    settings_read_int(GS, KEY_PREFER_CODEC, &codec, SX_RAW);
    return codec;
}

void SvpSettings::setPreferCodec(int codec)
{
    settings_write_int(GS, KEY_PREFER_CODEC, codec);
}

bool SvpSettings::isLZ4HC() const
{
    int enable;
    settings_read_int(GS, KEY_LZ4_HC, &enable, SX_RAW);
    return (enable != 0);
}

void SvpSettings::setLZ4HC(bool en)
{
    settings_write_int(GS, KEY_LZ4_HC, en ? 1 : 0);
}
