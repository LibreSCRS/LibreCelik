// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef LIBRECELIK_H
#define LIBRECELIK_H

#include "document/document.h"
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class LibreCelik; }
QT_END_NAMESPACE

class EId;
class SmartCardEvent;

class LibreCelik : public QMainWindow
{
    Q_OBJECT
    
public:
    LibreCelik(QWidget *parent = nullptr);
    ~LibreCelik();

private slots:
    void onCardEventReceived(const SmartCardEvent& sce);
    void onSmartCardReaderEnumerationChanged(const QStringList& scrNames);

private:
    // void addNewEIdReader(std::string reader);
    // void removeEIdReader(std::string reader);

    void addNewReader(std::string reader);
    void removeReader(std::string reader);
    
private:
    Ui::LibreCelik *ui;

    using DocumentReaders = std::map<std::string, Document*>;
    DocumentReaders documentReaders;

    using EIdReaders = std::map<std::string, EId*>;
    EIdReaders eIdReaders;
};
#endif // LIBRECELIK_H
