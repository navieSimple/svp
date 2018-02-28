#ifndef SETTINGSITEMDELEGATE_H
#define SETTINGSITEMDELEGATE_H

#include <QStyledItemDelegate>

class SettingsItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    SettingsItemDelegate(QWidget *parent = 0);
    ~SettingsItemDelegate();

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

};

#endif // SETTINGSITEMDELEGATE_H
