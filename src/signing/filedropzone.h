// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QStringList>
#include <QWidget>

class FileDropZone : public QWidget
{
    Q_OBJECT
public:
    explicit FileDropZone(QWidget* parent = nullptr);

    QStringList filePaths() const;
    void clear();
    void addFiles(const QStringList& paths);
    void removeFile(const QString& path);

signals:
    void filesChanged(const QStringList& paths);

protected:
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void browseFiles();

    QStringList files;
    bool dragOver = false;
};
