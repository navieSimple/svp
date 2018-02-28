#ifndef IDECODER_H
#define IDECODER_H

#include "svp.h"
#include "net_packet.h"
#include <QObject>
#include <QString>

class IDecoder
{
public:
    virtual int id() const = 0;
    virtual QString name() const = 0;

    virtual int init() = 0;
    virtual void fini() = 0;
    virtual bool decode(pac_bitmap *in, QByteArray &out) = 0;
};

#define IDecoder_iid "org.cvm.SVP.Client.IDecoder"
Q_DECLARE_INTERFACE(IDecoder, IDecoder_iid)

#endif // IDECODER_H
