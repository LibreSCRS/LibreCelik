// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef PRINTMANAGER_H
#define PRINTMANAGER_H

#include <QString>
#include <QPrinter>
#include <QPrintDialog>

template <typename T>
concept Printable = requires(T t, QPrinter* printer) {
    { t.print(printer) } -> std::same_as<void>;
};

class PrintManager
{
public:
    PrintManager() = delete;


    template<typename T>
    requires Printable<T>
    static void printDocument(const T& document, QString dialogTitle)
    {
        QPrinter printer;
        QPrintDialog dialog(&printer);
        dialog.setWindowTitle(dialogTitle);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        document.print(&printer);
    }
};

#endif // PRINTMANAGER_H
