// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QFile>
#include <QFontDatabase>
#include "utils/libreceliklog.h"
#include "textdocument.h"
#include "config.h"

void TextDocument::print(QPrinter *printer) const
{
    qCDebug(libreCelikPrinting) << "Printer page rect: " << printer->pageRect(QPrinter::Unit::Point).size();
    QMarginsF margins(21, 29.7, 21, 29.7);
    printer->setPageMargins(margins, QPageLayout::Millimeter);
    document.print(printer);
}

QString TextDocument::loadFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCDebug(libreCelikPrinting) << "File not opened in readonly:" << path;
        return {};
    }
    return file.readAll();
}

void TextDocument::setupDocument(const QString& htmlContent, const QString& cssPath)
{
    document.setPageSize(QPageSize::size(QPageSize::A4, QPageSize::Unit::Point));
    document.setHtml(htmlContent);
    document.setDefaultStyleSheet(loadFile(cssPath));

    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(isMacOS() ? MACOS_FONTSIZE : LINUX_FONTSIZE);
    document.setDefaultFont(font);

    qCDebug(libreCelikPrinting) << "Document page size: " << document.pageSize();
    qCDebug(libreCelikPrinting) << "Page count: " << document.pageCount();
}

QString TextDocument::getPreparedValue(const QString& data) const
{
    return (data.isEmpty() || data == "01.01.0001") ? qtTrId("lc-doc-unavailable") : data;
}

bool TextDocument::isMacOS()
{
    return LIBRECELIK_SYSTEM_NAME == MACOS;
}

bool TextDocument::isLinux()
{
    return LIBRECELIK_SYSTEM_NAME == LINUX;
}
