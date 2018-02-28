#ifndef KEYBOARDTHREAD_H
#define KEYBOARDTHREAD_H

#include <QThread>
#include <QWidget>

class SvpWidget;
class KeyboardThread : public QThread
{
    Q_OBJECT
public:
    explicit KeyboardThread(SvpWidget *window, QObject *parent = 0);
    ~KeyboardThread();

    void run();

signals:

};

#endif // KEYBOARDTHREAD_H
