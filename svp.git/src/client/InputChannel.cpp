#include "InputChannel.h"
#include "MainWindow.h"
#ifdef Q_OS_WIN
#include "KeyboardThread.h"
#endif

class InputChannel::Private : public QObject
{
    Q_OBJECT

public:
    Private(InputChannel *_q) : q(_q) {}

    Q_INVOKABLE void sendMouseEvent(const QPoint &pos, qint16 buttons)
    {
        QByteArray buf;
        pac_hdr cmd;
        pac_mouse_event mouse;

//        qDebug() << Q_FUNC_INFO << currentThread() << pos << buttons;
        cmd.c_magic = SVP_MAGIC;
        cmd.c_cmd = SC_MOUSE_EVENT;
        cmd.c_size = sizeof(mouse);
        buf.append((const char *)&cmd, sizeof(cmd));

        mouse.x = pos.x();
        mouse.y = pos.y();
        mouse.buttons = buttons;
        buf.append((const char *)&mouse, sizeof(mouse));

        q->write((const uint8_t *)buf.constData(), buf.size());
    }

    Q_INVOKABLE void sendKeyEvent(quint32 scancode, bool down)
    {
        QByteArray buf;
        pac_hdr cmd;
        pac_keyboard_event kbd;

        cmd.c_magic = SVP_MAGIC;
        cmd.c_cmd = SC_KEYBOARD_EVENT;
        cmd.c_size = sizeof(kbd);
        buf.append((const char *)&cmd, sizeof(cmd));

        kbd.down = down ? 1 : 0;
        kbd.scancode = scancode;
        buf.append((const char *)&kbd, sizeof(kbd));

        q->write((const uint8_t *)buf.constData(), buf.size());
    }

    InputChannel *q;
#ifdef Q_OS_WIN
    KeyboardThread *kbdThread;
#endif
};

InputChannel::InputChannel(const QHostAddress &addr, uint16_t port, QObject *parent)
    : Channel(addr, port ,parent)
    , d(0)
{
    setType(SH_INPUT);
    connect(this, SIGNAL(connected()), SLOT(negotiate()), Qt::DirectConnection);
    connect(this, SIGNAL(negotiated(uint32_t)), SLOT(process()), Qt::DirectConnection);
}

InputChannel::~InputChannel()
{
    delete d;
}

void InputChannel::mouseInput(const QPoint &pos, Qt::MouseButtons buttons)
{
//    qDebug() << Q_FUNC_INFO << currentThread() << pos << buttons << isRunning();
    if (!d || !isRunning())
        return;
    metaObject()->invokeMethod(d, "sendMouseEvent", Qt::QueuedConnection,
                               Q_ARG(QPoint, pos), Q_ARG(qint16, (qint16)buttons));
}

void InputChannel::keyboardInput(uint32_t scancode, bool down)
{
    if (!d || !isRunning())
        return;
    metaObject()->invokeMethod(d, "sendKeyEvent", Qt::QueuedConnection,
                               Q_ARG(quint32, scancode), Q_ARG(bool, down));
}

void InputChannel::negotiate()
{
    genericNegotiate((int)SH_INPUT, 0, ~0);
}

void InputChannel::process()
{
#ifdef Q_OS_WIN
    d->kbdThread->start();
#endif
}

void InputChannel::run()
{
    SvpWidget *w = MainWindow::instance()->svpWidget();
    Q_ASSERT(w);
    d = new InputChannel::Private(this);
#ifdef Q_OS_WIN
    d->kbdThread = new KeyboardThread(w);
#endif

    Channel::run();

#ifdef Q_OS_WIN
    delete d->kbdThread;
#endif
    delete d;
}

#include "InputChannel.moc"
