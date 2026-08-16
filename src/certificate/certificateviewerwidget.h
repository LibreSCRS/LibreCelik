// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QItemSelection>
#include <QWidget>

namespace Ui {
class CertificateViewerWidget;
}

class CertificateViewerWidget : public QWidget
{
    Q_OBJECT
public:
    /// @brief Build a viewer for one certificate as the agent described it.
    ///
    /// No DER is parsed here — or anywhere in this process. Everything the
    /// three tabs show comes from @p cert: its typed members and the agent's
    /// own grouped field dictionary.
    explicit CertificateViewerWidget(const LibreSCRS::AgentClient::CertificateInfo& cert, QWidget* parent = nullptr);
    ~CertificateViewerWidget() override;

public slots:
    /// @brief Hand over the raw certificate bytes fetched from the agent.
    ///
    /// Used for one thing only: the forensic hex dump shown under the parse
    /// error when the agent could not decode the certificate. For a
    /// certificate the agent DID decode, the detail rows are the agent's own
    /// and these bytes change nothing on screen — they stay reachable through
    /// the viewer's export action.
    void setForensicDer(const QByteArray& der);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onDetailsSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onCertPathSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private:
    void retranslateUi();
    void populateGeneralTab();
    void populateUnparseableTab();
    void installDetailsModel();

    Ui::CertificateViewerWidget* ui;
    LibreSCRS::AgentClient::CertificateInfo certificate;
    QByteArray forensicDer;
    bool unparseable = false;
};
