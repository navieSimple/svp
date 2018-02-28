#include "ChannelManager.h"
#include "MainChannel.h"
//#include "InfoWidget.h"
#include "SvpRecorder.h"
#include "ConnectDialog.h"
#include "MainWindow.h"
#include <QApplication>
#include <QTime>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ChannelManager *channelManager = new ChannelManager;
//    InfoWidget panel;
    SvpRecorder *recorder;
    ConnectDialog dialog;
    MainWindow mwin;

    QObject::connect(&dialog, SIGNAL(rejected()), qApp, SLOT(quit()));
    QObject::connect(&dialog, SIGNAL(accepted()), &mwin, SLOT(show()));
    QObject::connect(&dialog, SIGNAL(connectRequest(QString,quint16)),
                     channelManager, SLOT(start(QString,quint16)));

    QObject::connect(channelManager, SIGNAL(mainChannelConnected()), &dialog, SLOT(accept()));
    QObject::connect(channelManager, SIGNAL(mainChannelError()), &dialog, SLOT(cancelConnect()));

    if (app.arguments().contains("-record")) {
        recorder = new SvpRecorder;
        recorder->setDir(QTime::currentTime().toString("record_HH时mm分"));
    }

    dialog.show();
//    panel.show();
    return app.exec();
}
