#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "SvpWidget.h"
#include "SettingsDialog.h"
#include "settings.h"
#include "usb/USBMonitor.h"
#include "usb/USBDevice.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QToolButton>
#include <QDebug>

static MainWindow *g_mainWindow = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QWidget *empty = new QWidget(this);
    empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->toolBar->insertWidget(ui->actionStatistics, empty);

    m_scaleActionGroup = new QActionGroup(this);
    m_scaleActionGroup->setExclusive(true);
    m_scaleActionGroup->addAction(ui->actionRemote);
    m_scaleActionGroup->addAction(ui->actionCenter);
    m_scaleActionGroup->addAction(ui->actionClient);

    QMenu *scaleMenu = new QMenu(this);
    scaleMenu->addAction(ui->actionRemote);
    scaleMenu->addAction(ui->actionCenter);
    scaleMenu->addAction(ui->actionClient);
    ui->actionScale->setMenu(scaleMenu);

    QToolButton *scaleButton = new QToolButton(this);
    scaleButton->setPopupMode(QToolButton::InstantPopup);
    scaleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    scaleButton->setMenu(scaleMenu);
    scaleButton->setDefaultAction(ui->actionScale);
    scaleButton->setFocusPolicy(Qt::NoFocus);

    QAction *scaleAction = ui->toolBar->insertWidget(ui->actionFullScreen, scaleButton);

    m_usbMenu = new QMenu(this);
    m_usbMenu->addAction(ui->actionEmpty);
    ui->actionUSB->setMenu(m_usbMenu);

    QToolButton *usbButton = new QToolButton(this);
    usbButton->setPopupMode(QToolButton::InstantPopup);
    usbButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    usbButton->setMenu(m_usbMenu);
    usbButton->setDefaultAction(ui->actionUSB);
    usbButton->setFocusPolicy(Qt::NoFocus);

    ui->toolBar->insertWidget(scaleAction, usbButton);

    g_mainWindow = this;

    m_svpWidget = new SvpWidget;
    ui->scrollArea->setWidget(m_svpWidget);

    ui->actionCenter->setChecked(true);

    updateUSBDeviceList();

    connect(ui->actionConfigure, SIGNAL(triggered()), SLOT(showSettingsDialog()));
    connect(USBMonitor::instance(), SIGNAL(deviceListChanged()), SLOT(updateUSBDeviceList()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

MainWindow *MainWindow::instance()
{
    Q_ASSERT(g_mainWindow);
    return g_mainWindow;
}

SvpWidget *MainWindow::svpWidget()
{
    return m_svpWidget;
}

void MainWindow::showSettingsDialog()
{
    if (SettingsDialog::instance()->exec() == QDialog::Accepted)
        emit settingsChanged();
}

void MainWindow::updateUSBDeviceList()
{
    QList<USBDevice *> dlist = USBMonitor::instance()->getDeviceList();
    qDeleteAll(m_usbDeviceActions);
    m_usbDeviceActions.clear();
    m_usbMenu->clear();
    if (dlist.empty()) {
        m_usbMenu->addAction(ui->actionEmpty);
    } else {
        foreach (USBDevice *device, dlist) {
            QString name = QString("%1 [%2] %3:%4").arg(device->productName)
                    .arg(device->vendorName)
                    .arg(device->vid, 4, 16, QLatin1Char('0'))
                    .arg(device->pid, 4, 16, QLatin1Char('0'));
            QAction *action = m_usbMenu->addAction(name, this, SLOT(selectUSBDevice()));
            action->setCheckable(true);
            action->setChecked(false);
            action->setData(device->id);
            m_usbDeviceActions.append(action);
            m_usbMenu->addAction(action);
        }
    }
    USBMonitor::instance()->putDeviceList(dlist);
}

void MainWindow::selectUSBDevice()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action) {
        qDebug() << Q_FUNC_INFO << "not called from signal handler";
        return;
    }
    uint id = action->data().toUInt();
    if (action->isChecked())
        action->setChecked(USBMonitor::instance()->attachDevice(id));
    else
        USBMonitor::instance()->detachDevice(id);
}
