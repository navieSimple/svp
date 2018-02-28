#include "ConnectDialog.h"
#include "ui_ConnectDialog.h"
#include "SettingsDialog.h"
#include <QApplication>
#include <QKeyEvent>
#include <QDebug>

#define DEFAULT_PORT 5678

ConnectDialog::ConnectDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConnectDialog)
{
    ui->setupUi(this);
    Qt::WindowFlags flags = windowFlags();
#ifdef Q_OS_WIN
    flags |= Qt::MSWindowsFixedSizeDialogHint;
#endif
    flags |= Qt::WindowMinimizeButtonHint;
    flags &= ~Qt::WindowContextHelpButtonHint;
    setWindowFlags(flags);

    connect(ui->optionButton, SIGNAL(clicked()), SLOT(showSettingsDialog()));
    connect(ui->connectButton, SIGNAL(clicked()), SLOT(startConnect()));

    ui->addrComboBox->installEventFilter(this);
}

ConnectDialog::~ConnectDialog()
{
    delete ui;
}

bool ConnectDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->addrComboBox && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
            if (ui->connectButton->isEnabled())
                startConnect();
        }
    }
    return false;
}

void ConnectDialog::cancelConnect()
{
    ui->connectButton->setText("连接(&C)");
    ui->connectButton->setEnabled(true);
}

void ConnectDialog::showSettingsDialog()
{
    SettingsDialog::instance()->exec();
}

void ConnectDialog::startConnect()
{
    ui->connectButton->setEnabled(false);
    ui->connectButton->setText("连接中...");
    if (parseAddr(ui->addrComboBox->currentText()))
        emit connectRequest(m_addr, m_port);
}

bool ConnectDialog::parseAddr(const QString &addr)
{
    int idx = addr.lastIndexOf(':');
    bool ok = true;
    if (idx != -1) {
        m_addr = addr.left(idx);
        m_port = addr.midRef(idx + 1).toUShort(&ok);
    } else {
        m_addr = addr;
        m_port = DEFAULT_PORT;
    }
    if (ok)
        emit connectRequest(m_addr, m_port);
}
