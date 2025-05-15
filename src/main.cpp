// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "config.h"
#include "utils/libreceliklog.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

using namespace std::literals;
const static char* LOGPATTERN = "\033[32m[%{time yyyyMMdd h:mm:ss.zzz ttt} %{if-debug}DEBUG%{endif}%{if-info}INFO%{endif}%{if-warning}WARNING%{endif}%{if-critical}CRITICAL%{endif}%{if-fatal}F%{endif}]%{if-category}\033[36m %{category}:%{endif} \033[37m %{threadid} %{if-debug}\033[34m%{function}%{endif}%{if-warning}\033[31m%{backtrace depth=3}%{endif}%{if-critical}\033[31m%{backtrace depth=3}%{endif}%{if-fatal}\033[31m%{backtrace depth=3}%{endif}\033[0m %{message}";

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    qSetMessagePattern(LOGPATTERN);

    qCInfo(libreSCRSGeneral) << "Starting LibreCelik. Version: " << LIBRECELIK_VERSION;

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "LibreCelik_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    LibreCelik w;
    w.show();

    return a.exec();
}
