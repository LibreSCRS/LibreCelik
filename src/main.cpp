// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "librecelik.h"
#include "config.h"
#include "smartcard/smartcardreaderlistener.h"
#include "utils/libreceliklog.h"

#include <LibreSCRS/Secure/String.h>

#include <QApplication>
#include <QIcon>
#include <QMetaType>

using namespace std::literals;
const static char* LOGPATTERN =
    "\033[32m[%{time yyyyMMdd h:mm:ss.zzz ttt} "
    "%{if-debug}DEBUG%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}CRITICAL%{endif}%{if-"
    "fatal}F%{endif}]%{if-category}\033[36m %{category}:%{endif} \033[37m %{threadid} "
    "%{if-debug}\033[34m%{function}%{endif}%{if-warning}\033[31m%{backtrace "
    "depth=3}%{endif}%{if-critical}\033[31m%{backtrace depth=3}%{endif}%{if-fatal}\033[31m%{backtrace "
    "depth=3}%{endif}\033[0m %{message}";

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    qSetMessagePattern(LOGPATTERN);
    a.setWindowIcon(QIcon(":/images/smartcard-id-512.png"));

    // PIN material flows through Qt signal/slot connections
    // (notably ChangePinDlg::pinChangeRequested) as Secure::String rather
    // than QString so that cleanse-on-destruction survives the
    // Qt::QueuedConnection marshalling. The metatype must be registered
    // once, before any signal carrying it is connected, otherwise
    // QueuedConnection invocation aborts.
    qRegisterMetaType<LibreSCRS::Secure::String>();

    qCInfo(lcGeneral) << "Starting LibreCelik - Version: " << LIBRECELIK_VERSION;
#if defined(LIBRECELIK_LOCAL_MIDDLEWARE_VERSION) && LIBRECELIK_LOCAL_MIDDLEWARE_VERSION
    qCInfo(lcGeneral) << "Using LibreMiddleware - Version: LOCAL";
#else
    qCInfo(lcGeneral) << "Using LibreMiddleware - Version: " << LIBRECELIK_MIDDLEWARE_VERSION;
#endif

    // SmartCardReaderListener is a Q_GLOBAL_STATIC (see
    // src/smartcard/smartcardreaderlistener.{h,cpp}) so QApplication tears it
    // down deterministically before its own destructor runs — no explicit
    // shutdown handler required.

    LibreCelik w;
    w.show();

    return a.exec();
}
