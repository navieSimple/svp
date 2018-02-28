#include "KeyboardThread.h"
#include "SvpWidget.h"
#include <QApplication>
#include <QDebug>
#include <QMetaObject>
#include <windows.h>

SvpWidget *g_window = 0;

LRESULT CALLBACK ll_kbd_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
    PKBDLLHOOKSTRUCT p;
    quint32 scanCode;

    if (g_window && g_window == QApplication::focusWidget() && nCode == HC_ACTION) {
        switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            p = (PKBDLLHOOKSTRUCT)lParam;
            if (!p)
                return TRUE;
            scanCode = p->scanCode;
            if (p->flags & LLKHF_EXTENDED)
                scanCode |= 0xe000;
//            qDebug() << hex << scanCode << "extended" << ((p->flags & LLKHF_EXTENDED) ? true : false);
            g_window->keyboardInput(scanCode, (p->flags & LLKHF_UP) ? false : true);
            /*
            QMetaObject::invokeMethod(g_window, "keyboardInput", Qt::QueuedConnection,
                                      Q_ARG(quint32, scanCode),
                                      Q_ARG(bool, (p->flags & LLKHF_UP) ? false : true));
                                      */
            return TRUE;
            break;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}


KeyboardThread::KeyboardThread(SvpWidget *window, QObject *parent)
{
    g_window = window;
}

KeyboardThread::~KeyboardThread()
{

}

void KeyboardThread::run()
{
    MSG msg;
    BOOL status;
    HHOOK hook_handle;

    hook_handle = SetWindowsHookEx(WH_KEYBOARD_LL, ll_kbd_proc, GetModuleHandle(0), 0);
    if (!hook_handle) {
        qDebug() << "SetWindowsHookEx error";
        return;
    }
    qDebug() << "keyboard hook success";
    exec();
    UnhookWindowsHookEx(hook_handle);
}
