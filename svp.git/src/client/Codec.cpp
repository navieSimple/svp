#include "Codec.h"
#include "svp.h"
#include "net_packet.h"
#include "IDecoder.h"
#include <QPluginLoader>
#include <QStringList>
#include <QDir>
#include <QDebug>
#include <QGlobalStatic>

Q_GLOBAL_STATIC(Codec, g_codec)

Codec::Codec(QObject *parent) :
    QObject(parent)
{
    QDir d = QDir::current();
    d.cd("../lib/codec");
    loadPlugins(d);
}

Codec::~Codec()
{

}

Codec *Codec::instance()
{
    return g_codec();
}

bool Codec::decode(pac_bitmap *in, QByteArray &out)
{
    IDecoder *dec = m_decoders.value(in->codec);
    if (!dec)
        return false;
    else
        return dec->decode(in, out);
}

void Codec::loadPlugins(const QDir &dir)
{
    qDebug() << "loading decoder plugin from " << dir.absolutePath();
    foreach (const QString &name, dir.entryList(QStringList() << "*.dll" << "*.so", QDir::Files)) {
        QPluginLoader loader(dir.absoluteFilePath(name));
        IDecoder *decoder = qobject_cast<IDecoder *>(loader.instance());
        if (decoder) {
            int rc = decoder->init();
            if (rc) {
                qDebug() << QString("init decoder %1 error, rc %2").arg(decoder->name()).arg(rc);
                loader.unload();
                continue;
            }
            m_decoders.insert(decoder->id(), decoder);
            qDebug() << QString("decoder %1 loaded").arg(decoder->name());
        } else {
            qDebug() << "loading" << name << "error:" << loader.errorString();
        }
    }
}
