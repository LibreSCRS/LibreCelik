// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#pragma once

#include <LibreSCRS/SmartCard/MonitorService.h>

#include <QMetaType>
#include <QObject>
#include <QStringList>

#include <set>
#include <string>

// Qt6 auto-registers copy-constructible POD-like types through the metatype
// system at first use. No Q_DECLARE_METATYPE needed for
// LibreSCRS::SmartCard::MonitorEvent — qRegisterMetaType at runtime is
// sufficient for QueuedConnection marshalling and QSignalSpy.

/// @brief Qt-side adapter that re-emits LibreSCRS::SmartCard::MonitorService events
///        across the GUI thread via QueuedConnection.
///
/// @par Ownership rationale
/// Takes the underlying MonitorService by raw `MonitorService&` rather than the LM 4.0
/// API's recommended `std::shared_ptr<MonitorService>`. The choice is deliberate
/// and isolated to LC: SmartCardReaderListener (the sole owner of the
/// MonitorService in this process) holds a `std::unique_ptr<MonitorService>` so there is
/// exactly one logical owner; sharing the MonitorService across multiple LC
/// observers would couple lifetimes the singleton-with-Q_GLOBAL_STATIC
/// shape is meant to avoid. The raw reference is safe because the
/// listener outlives every QSmartCardMonitor it constructs (LC creates
/// these on widget construction and destroys them on widget destruction;
/// the MonitorService itself is destroyed only when QApplication exits, after
/// the LibreCelik MainWindow has unwound).
///
/// LM hosts that genuinely share a MonitorService across decoupled subsystems
/// should follow the public API's `shared_ptr<MonitorService>` recommendation;
/// LC does not, and the unique-ownership choice is documented here so
/// future refactors don't silently widen the lifetime contract.
class QSmartCardMonitor : public QObject
{
    Q_OBJECT
public:
    explicit QSmartCardMonitor(LibreSCRS::SmartCard::MonitorService& monitor, QObject* parent = nullptr);
    ~QSmartCardMonitor() override;

signals:
    void cardEvent(const LibreSCRS::SmartCard::MonitorEvent& event);
    /// @brief Emitted once per individual ReaderAdded / ReaderRemoved event
    ///        from the underlying public MonitorService — NOT once per full-list
    ///        diff. On startup with N readers plugged, N signals fire in
    ///        sequence (each carrying a growing set). Consumers that care
    ///        only about the final membership should debounce or wait for
    ///        quiescence; `LibreCelik::onSmartCardReaderEnumerationChanged`
    ///        is idempotent and safely handles this.
    void readerListChanged(const QStringList& readers);

private:
    LibreSCRS::SmartCard::MonitorService& monitor;
    LibreSCRS::SmartCard::MonitorService::SubscriptionId subscriptionId{};
    // Tracks the set of currently known readers so we can synthesise a
    // full-list QStringList from the public MonitorService's per-reader
    // ReaderAdded / ReaderRemoved events. Touched only on the Qt main thread
    // (via QueuedConnection marshal in the callback).
    std::set<std::string> knownReaders;
};
