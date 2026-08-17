// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Everything the sign path decides WITHOUT an agent: the UI level
///        token, the visual-signature map, the partition into homogeneous
///        runs, and the consumption of one finished sign/signBatch result.
///
/// The consumption in particular lives here rather than inside the live
/// controller's finished handler because it is the load-bearing logic of the
/// whole batch path — which rows succeeded, which descriptors must be drained,
/// whether the remaining runs may still be dialled — and a handler that owns
/// it can only be proven against a real agent. As a pure function it is proven
/// in CI, against every observed wire behaviour, on a bench-less machine.

#pragma once

#include "agent/signcontroller.h"

#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/FdHandle.h>
#include <LibreSCRS/AgentClient/OperationPhase.h> // OperationStatus for consumeBatchOutcome
#include <LibreSCRS/AgentClient/SignOptions.h>

#include <QList>
#include <QRectF>
#include <QString>
#include <QVariantMap>

#include <vector>

namespace librecelik::agent {

/// UI level token ("B_B"/"B_T"/"B_LT"/"B_LTA", anything else → Auto).
[[nodiscard]] LibreSCRS::AgentClient::SignatureLevel levelFromUiToken(const QString& token);

/// The wire's required 6-key visualSignature map (SignOptions.h contract):
/// page (uint, 0-based), x/y/width/height (double, PDF user units), text.
[[nodiscard]] QVariantMap makeVisualSignatureMap(int page, const QRectF& box, const QString& text);

/// STABLE-SORT partition by (format, packaging) — NOT consecutive-only:
/// (a.pdf, b.xml, c.pdf) is TWO runs, never three (each extra run is an
/// extra consent + PIN presentation + rate-limiter charge; order-dependent
/// prompt counts are dishonest UX). Within a run, items keep selection
/// order; sourceIndexes maps run rows back to the selection-order result
/// rows the wizard displays.
struct SignRun
{
    QList<int> sourceIndexes;
    QList<SignRequestItem> items;
};
[[nodiscard]] QList<SignRun> partitionIntoRuns(const QList<SignRequestItem>& files);

/// The name one document is announced under — its file name, never its path.
/// The single source for it: the batch rows and the single sign's options must
/// name the same document the same way.
[[nodiscard]] QString documentDisplayName(const SignRequestItem& item);

/// The options ONE run is dialled with, derived from the request's @p options
/// and the run itself.
///
/// The KIND is explicit: a batch carries ONE format for every document in it
/// and the wire sniffs only the first, so it is set from the run rather than
/// inferred, and one file's kind can never be applied to all the others.
///
/// The DISPLAY NAME is set for a single-document run ONLY. It is not chrome
/// there: the ASiC-E container's data entry is named from it (a container
/// created without a name is refused) and the detached JAdES/XAdES reference
/// URI derives from it, so a byte-path single sign without it fails agent-side.
/// A batch names each document on its own `BatchDocument` row instead, and the
/// agent deliberately never reads the option off a SignBatch request.
[[nodiscard]] LibreSCRS::AgentClient::SignOptions runSignOptions(const LibreSCRS::AgentClient::SignOptions& options,
                                                                 const SignRun& run);

/// Ledger wire rule: rows carry NO status field — success is derived.
[[nodiscard]] bool batchRowSucceeded(LibreSCRS::AgentClient::ErrorCode rowError, qint64 artifactSize);

/// PURE consumption of one finished sign/signBatch op — the load-bearing
/// finished-handler logic, unit-testable against every ledger wire
/// observation without a live agent. Consumes EVERY row (fd drain is the
/// caller's side effect; emissions carry what to write/report).
struct RowEmission
{
    int sourceIndex = -1;                      ///< selection-order row
    bool ok = false;                           ///< batchRowSucceeded verdict
    LibreSCRS::AgentClient::FdHandle artifact; ///< engaged only when ok
    QString message;                           ///< localized failure text, empty when ok
};
struct BatchOutcome
{
    // std::vector, NOT QList: RowEmission carries a move-only FdHandle and
    // QList requires copy-constructible elements.
    std::vector<RowEmission> emissions; ///< one per row of the run — never fewer (drain-all rule)
    bool haltRemainingRuns = false;     ///< CredentialWrong/CredentialBlocked contagion
    bool retryableRateLimit = false;    ///< ErrorCode::RateLimited — surface, never auto-retry
};
[[nodiscard]] BatchOutcome consumeBatchOutcome(LibreSCRS::AgentClient::OperationStatus status,
                                               LibreSCRS::AgentClient::ErrorCode opError,
                                               std::vector<LibreSCRS::AgentClient::BatchSignRow> rows,
                                               const SignRun& run);

/// O_RDONLY open → FdHandle; invalid handle on failure.
[[nodiscard]] LibreSCRS::AgentClient::FdHandle openDocumentFd(const QString& filePath);

/// Output path: <folder>/<basename>-signed.<ext per format/packaging>
/// (preserves today's SignPage::buildOutputPath naming).
[[nodiscard]] QString outputPathFor(const SignRequestItem& item, const QString& outputFolder);

/// The one line for an artifact the agent signed but LC could not write out.
///
/// It lives with the sign helpers rather than in the wire-vocabulary mapper
/// next door because it is not a wire outcome at all: the failure happens
/// AFTER the agent is done, on this side of the seam, and no `ErrorCode`
/// names it. Fetched at call time like every other string in this codebase —
/// a language switch must change what the next call returns.
[[nodiscard]] QString artifactWriteFailedText();

} // namespace librecelik::agent
