#ifndef SVPSETTINGS_H
#define SVPSETTINGS_H

#include "svp.h"
#include <QByteArray>

class SvpSettings
{
public:
    SvpSettings();
    ~SvpSettings();

    static SvpSettings *instance();

    QByteArray dump() const;
    void save();

    int preferCodec() const;
    void setPreferCodec(int codec);

    bool isLZ4HC() const;
    void setLZ4HC(bool en);

};

#endif // SVPSETTINGS_H
