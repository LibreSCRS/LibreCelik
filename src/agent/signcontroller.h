// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <LibreSCRS/AgentClient/SignOptions.h>
#include <QObject>

namespace librecelik::agent {

struct SignRequestItem
{
    QString filePath;
    LibreSCRS::AgentClient::SignatureFormat format = LibreSCRS::AgentClient::SignatureFormat::PAdES;
    LibreSCRS::AgentClient::Packaging packaging = LibreSCRS::AgentClient::Packaging::Enveloped;
};

struct SignRowResult
{
    QString filePath;
    QString outputPath; ///< written artifact, empty on failure
    bool ok = false;
    QString message; ///< localized failure text, empty on success
};

class SignController : public QObject
{
    Q_OBJECT
public:
    explicit SignController(QObject* parent = nullptr);
    ~SignController() override;

    /// DELIBERATELY stricter than the client's own dial gate (which requires
    /// only "visual-sign"): LC refuses to OFFER a visual signature it cannot
    /// PREVIEW, so the placement page and the signed output never disagree.
    [[nodiscard]] virtual bool canVisualSign() const = 0;  // "visual-sign" && "layout-preview"
    [[nodiscard]] virtual bool canTsaOverride() const = 0; // "tsa-url"
    [[nodiscard]] virtual bool canBatch() const = 0;       // "batch-sign"

    /// Stable-sort partitions files into homogeneous (format, packaging)
    /// runs (interleaved kinds coalesce — prompt count never depends on
    /// selection order); each run of >= 2 goes through signBatch (one
    /// consent+PIN per run) when canBatch(), else degrades to sequential
    /// per-file sign() (N prompts — today's behavior); a run of 1 through
    /// sign(). options.format/packaging are taken per-item; the rest of
    /// options (level, visualSignature, tsaUrl, extra) applies to all.
    virtual void start(const QString& certId, const QList<SignRequestItem>& files,
                       const LibreSCRS::AgentClient::SignOptions& options, const QString& outputFolder) = 0;
    virtual void cancel() = 0;

signals:
    void phaseChanged(LibreSCRS::AgentClient::OperationPhase phase, double progress);
    void rowFinished(int index, const librecelik::agent::SignRowResult& result);
    void finished(int succeeded, int failed);
};

} // namespace librecelik::agent
