// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "config.h"
#include "utils/libreceliklog.h"

#include <QApplication>
#include <QIcon>

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

    qCInfo(libreSCRSGeneral) << "Starting LibreCelik - Version: " << LIBRECELIK_VERSION;
#if defined(LIBRECELIK_LOCAL_MIDDLEWARE_VERSION) && LIBRECELIK_LOCAL_MIDDLEWARE_VERSION
    qCInfo(libreSCRSGeneral) << "Using LibreMiddleware - Version: LOCAL";
#else
    qCInfo(libreSCRSGeneral) << "Using LibreMiddleware - Version: " << LIBRECELIK_MIDDLEWARE_VERSION;
#endif

    LibreCelik w;
    w.show();

    return a.exec();
}
