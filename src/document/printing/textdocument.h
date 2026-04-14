// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QPrinter>
#include <QTextDocument>
#include <QCoreApplication>

class TextDocument
{
    Q_DECLARE_TR_FUNCTIONS(TextDocument)

public:
    void print(QPrinter* printer) const;

protected:
    QTextDocument document;

    // Load an HTML template or CSS file from a Qt resource / filesystem path.
    static QString loadFile(const QString& path);

    // Set up the QTextDocument: page size, HTML content, stylesheet, font.
    // Call from subclass constructors after translate + prepare are done.
    void setupDocument(const QString& htmlContent, const QString& cssPath);

    QString getPreparedValue(const QString& data) const;

    static bool isMacOS();

    inline static const std::string MACOS = "Darwin";
    inline static const int MACOS_FONTSIZE = 12;
    inline static const int LINUX_FONTSIZE = 8;
};
