#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>

namespace Ui {
class MainWindow;
}

class QAction;
class QActionGroup;
class QMenu;
class SvpWidget;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static MainWindow *instance();

    SvpWidget *svpWidget();

signals:
    void settingsChanged();

public slots:
    void showSettingsDialog();
    void updateUSBDeviceList();
    void selectUSBDevice();

private:
    Ui::MainWindow *ui;
    SvpWidget *m_svpWidget;
    QActionGroup *m_scaleActionGroup;
    QMenu *m_usbMenu;
    QList<QAction *> m_usbDeviceActions;
};

#endif // MAINWINDOW_H
