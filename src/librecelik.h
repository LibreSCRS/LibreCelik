// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef LIBRECELIK_H
#define LIBRECELIK_H

#include "document/document.h"
#include <QMainWindow>
#include <QTranslator>

QT_BEGIN_NAMESPACE
namespace Ui {
class LibreCelik;
}
QT_END_NAMESPACE

class SmartCardEvent;

class LibreCelik : public QMainWindow
{
    Q_OBJECT

public:
    LibreCelik(QWidget* parent = nullptr);
    ~LibreCelik();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onCardEventReceived(const SmartCardEvent& sce);
    void onSmartCardReaderEnumerationChanged(const QStringList& scrNames);
    void onLanguageChanged(int index);

private:
    void addNewReader(std::string reader, int retryCount = 0);
    void removeReader(std::string reader);
    bool loadLanguage(const QString& locale);
    void updateAboutText();
    void updateWelcomeChips();

private:
    Ui::LibreCelik* ui;

    using DocumentReaders = std::map<std::string, Document*>;
    DocumentReaders documentReaders;

    QTranslator translator;
    bool uiReady = false;
    QString locale;
};
#endif // LIBRECELIK_H
