#include "IDecoder.h"
#include <QtPlugin>

class LZ4Decoder : public QObject, public IDecoder
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.cvm.SVP.Client.IDecoder" FILE "LZ4Decoder.json")
    Q_INTERFACES(IDecoder)
public:
     LZ4Decoder();
     ~LZ4Decoder();

     int id() const;
     QString name() const;

     int init();
     void fini();
     bool decode(pac_bitmap *in, QByteArray &out);

private:
     char *m_buf;
};
