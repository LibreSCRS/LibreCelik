// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief One LibreCelik per user: a second start brings the first one's
///        window up and exits.
///
/// Two windows would each hold their own connection to the agent and read
/// the same card twice, and a desktop that starts LibreCelik from another
/// place (a panel, a file manager) would open a new window every time. The
/// first instance listens on a local socket that belongs to the user; a later
/// start that reaches it asks it to show itself and stops there.

#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace librecelik::utils {

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    /// @param name The local socket name or path. Use `defaultName()` in the
    ///             application; a test passes a path of its own.
    explicit SingleInstance(QString name, QObject* parent = nullptr);
    ~SingleInstance() override;

    /// The user's socket: in the user's runtime directory, so two users of one
    /// machine each get their own LibreCelik.
    [[nodiscard]] static QString defaultName();

    /// Become the instance, or hand over to the one already running.
    /// @return true when this process is the instance and should carry on;
    ///         false when another one was asked to show itself and this
    ///         process should exit.
    [[nodiscard]] bool claim();

signals:
    /// A later start asked this instance to show its window.
    void activationRequested();

private:
    [[nodiscard]] bool askRunningInstance() const;
    [[nodiscard]] bool listen();

    QString name;
    QLocalServer* server = nullptr;
};

} // namespace librecelik::utils
