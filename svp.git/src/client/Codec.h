#ifndef CODEC_H
#define CODEC_H

#include <QObject>
#include <QDir>
#include <QHash>

struct pac_bitmap;
class IDecoder;
class Codec : public QObject
{
    Q_OBJECT
public:
    explicit Codec(QObject *parent = 0);
    ~Codec();

    static Codec *instance();

    bool decode(pac_bitmap *in, QByteArray &out);

private:
    void loadPlugins(const QDir &dir);

    QHash<int, IDecoder *> m_decoders;
};

#endif // CODEC_H
