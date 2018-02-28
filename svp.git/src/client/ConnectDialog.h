#ifndef CONNECTDIALOG_H
#define CONNECTDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class ConnectDialog;
}

class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectDialog(QWidget *parent = 0);
    ~ConnectDialog();

    bool eventFilter(QObject *obj, QEvent *event);

signals:
    void connectRequest(const QString &addr, quint16 port);

public slots:
    void startConnect();
    void cancelConnect();
    void showSettingsDialog();

private:
    bool parseAddr(const QString &addr);

    Ui::ConnectDialog *ui;
    QString m_addr;
    quint16 m_port;
};

#endif // CONNECTDIALOG_H
