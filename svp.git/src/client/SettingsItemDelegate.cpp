#include "SettingsItemDelegate.h"
#include <QPainter>
#include <QListWidgetItem>
#include <QVariant>
#include <QIcon>
#include <QApplication>
#include <QStyle>
#include <QDebug>

static const int ICON_SIZE = 48;
static const int WIDTH = 60;
static const int HEIGHT = 72;

SettingsItemDelegate::SettingsItemDelegate(QWidget *parent)
    : QStyledItemDelegate(parent)
{
}

SettingsItemDelegate::~SettingsItemDelegate()
{

}

void SettingsItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QString name = index.data(Qt::DisplayRole).toString();
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QRect rect = option.rect;
    QPoint p(rect.left() + (rect.width() - ICON_SIZE) / 2, rect.top());
    painter->setClipRect(option.rect);

    if (option.state & QStyle::State_Selected) {
        painter->setBrush(option.palette.highlight());
        painter->drawRoundedRect(rect, 2, 2);

        painter->drawPixmap(p, icon.pixmap(ICON_SIZE, ICON_SIZE, QIcon::Selected));
        painter->setPen(option.palette.color(QPalette::HighlightedText));
        painter->drawText(rect.adjusted(0, ICON_SIZE + 2, 0, 0), Qt::AlignHCenter, name);
        return;
    }

    painter->drawPixmap(p, icon.pixmap(ICON_SIZE, ICON_SIZE, QIcon::Normal));
    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(rect.adjusted(0, ICON_SIZE + 2, 0, 0), Qt::AlignHCenter, name);

    painter->setClipping(false);
}

QSize SettingsItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QSize(WIDTH, HEIGHT);
}
