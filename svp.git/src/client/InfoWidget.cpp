#include "InfoWidget.h"
#include "ui_InfoWidget.h"
#include "ChannelManager.h"
#include "CursorChannel.h"
#include <QBitmap>
#include <QPixmap>
#include <QImage>
#include <svp.h>

InfoWidget::InfoWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InfoWidget)
    , m_cursor(0)
{
    ui->setupUi(this);

    connect(ChannelManager::instance(), SIGNAL(channelCreated(Channel*)), SLOT(onChannelCreated(Channel*)));
}

InfoWidget::~InfoWidget()
{
    delete ui;
}

void InfoWidget::cursorChanged(pac_cursor *cursor, int size)
{
    uint8_t *data = (uint8_t *)cursor + sizeof(*cursor);
    QBitmap mask = QBitmap::fromData(QSize(cursor->w, cursor->h), data, QImage::Format_Mono);
    QImage image(data + cursor->mask_len, cursor->w, cursor->h, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->cursorBitmap->setPixmap(QBitmap(pixmap));
    ui->cursorMask->setPixmap(mask);
    pixmap.setMask(mask);
    ui->cursorComposed->setPixmap(pixmap);
    uint h = qHash(QByteArray::fromRawData((const char *)data + cursor->mask_len, cursor->w * cursor->h * 3), 0);
    ui->cursorInfo->setText(QStringLiteral("Hash %1").arg(h, 0, 16));
}

void InfoWidget::onChannelCreated(Channel *channel)
{
    if (channel->type() != SH_CURSOR)
        return;
    m_cursor = qobject_cast<CursorChannel *>(channel);

    connect(m_cursor, SIGNAL(cursorCommand(pac_cursor*,int)), SLOT(cursorChanged(pac_cursor*,int)),
            Qt::BlockingQueuedConnection);
}
