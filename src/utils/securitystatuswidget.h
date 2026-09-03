// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

#include <LibreSCRS/AgentClient/SecurityChecks.h>

#include <QEvent>
#include <QList>
#include <QString>
#include <QStringView>
#include <QWidget>

#include <cstdint>
#include <optional>

class QLabel;
class QVBoxLayout;
class CollapsibleSection;

namespace librecelik::utils {

// MIRROR-OF: LibreAgent/client/qt/include/LibreSCRS/AgentClient/SecurityChecks.h
// - the name is re-exported into this host's namespace so its own call sites
// keep reading as they did. The enumeration itself has one definition, in the
// client library that reads the wire shape; this host owns how it is drawn,
// which is a different question and stays here.
using SecurityCategory = LibreSCRS::AgentClient::SecurityCategory;

/// @brief One security-verification check result (e.g. passive auth, chip auth).
struct SecurityCheck
{
    /// @brief Outcome of a single security check.
    ///
    /// Defined in the client library that decodes the wire token; named here
    /// so this widget's call sites go on saying `SecurityCheck::Status`.
    using Status = LibreSCRS::AgentClient::SecurityCheckStatus;

    QString checkId;                                     ///< Identifier for the check.
    SecurityCategory category = SecurityCategory::Other; ///< Category classification.
    Status status = Status::NotPerformed;                ///< Check outcome.
    QString label;                                       ///< Short human-readable label.
    QString detail;                                      ///< Supplemental detail rendered next to the outcome.
    QString errorDetail;                                 ///< Populated when @ref status is @ref Status::Failed.
    /// @brief Why the check ended the way it did, as a stable key.
    ///
    /// Empty for every check that simply ran. The reader that judges a travel
    /// document's signer against this installation's trust anchors names its
    /// outcome with a token ("csca.not-configured") rather than a sentence,
    /// because the same outcome has to reach a Serbian and an English holder
    /// and only the host knows which. @ref localizedReasonText turns it into
    /// words; nothing else may render it raw.
    QString reason;
};

/// @brief Aggregate security evaluation over a set of @ref SecurityCheck entries.
///
/// The three overall verdicts arrive already aggregated — nothing host-side
/// recomputes them, so this is a pure carrier.
struct SecurityStatusModel
{
    QList<SecurityCheck> checks;                                                     ///< Individual check results.
    SecurityCheck::Status overallIntegrity = SecurityCheck::Status::NotPerformed;    ///< DataIntegrity verdict.
    SecurityCheck::Status overallAuthenticity = SecurityCheck::Status::NotPerformed; ///< Authenticity verdict.
    SecurityCheck::Status overallGenuineness = SecurityCheck::Status::NotPerformed;  ///< Genuineness verdict.
};

// The two decoders moved into the client library, which is the one reader of
// this wire shape; this host names them so its call sites are unchanged.
using LibreSCRS::AgentClient::categoryFromString;
using LibreSCRS::AgentClient::statusFromString;

// statusToString() and categoryToString() are gone rather than moved. They
// were declared here and called from nowhere: the wire is decoded on the way
// in and never re-encoded on the way out of a viewer.

/// @brief Localized display text for a status.
///
/// Free rather than a member because two surfaces render a verdict now: this
/// pane, and the compact strip an annex section carries. A second copy of the
/// switch is how the two would drift into disagreeing about what NOT_PERFORMED
/// is called.
[[nodiscard]] QString localizedStatusText(SecurityCheck::Status status);

/// @brief The dot colour a status is drawn with. Same reason as above.
///
/// NotPerformed is deliberately GREY, not red: a check nobody ran is neither a
/// failure nor a pass, and painting it red would accuse the card of something.
[[nodiscard]] QString statusColorHex(SecurityCheck::Status status);

/// @brief Localized sentence for a @ref SecurityCheck::reason key.
///
/// Resolution, first match wins: an empty key stays empty (the ordinary case —
/// a check that simply ran carries no reason); a key this build names renders
/// its catalogue string; anything else renders the key VERBATIM.
///
/// That last arm is the whole point. A reader newer than this build can name a
/// reason nobody here has heard of, and the two tempting answers are both
/// worse than the key: dropping the line costs the holder the only record that
/// their document's signer went unchecked, and substituting "unknown" trades a
/// token a support report can act on for a word that says nothing. Same rule
/// the field grid already applies to a label key it does not recognise.
[[nodiscard]] QString localizedReasonText(const QString& reasonKey);

/// @brief One compact "label: status" row with a coloured dot.
///
/// Ownership passes to the caller's layout.
[[nodiscard]] QWidget* makeStatusRow(const QString& label, SecurityCheck::Status status, QWidget* parent = nullptr);

/// @brief The reader's OWN last choice for the per-check block, or
///        @c std::nullopt while they have not made one.
///
/// Application-scope and deliberately not per-widget: every card read builds a
/// new pane, so a choice kept on the widget would die with the read that heard
/// it and the block would re-decide on the next card. Software that argues with
/// a person about a section they just closed is worse than software that never
/// moves it.
///
/// Not written to settings either. This is a choice about the document in front
/// of the reader, not a preference about the application, so it lasts exactly
/// as long as the session does.
[[nodiscard]] std::optional<bool> rememberedDetailChecksChoice();

/// @brief Record that the reader opened (@p expanded) or closed the block.
void rememberDetailChecksChoice(bool expanded);

/// @brief Drop the remembered choice, returning the block to its derived
///        default. The state is process-wide, so a test that toggles it has to
///        put it back or it reaches the next test.
void forgetDetailChecksChoice();

/// @brief Put this host's VOCABULARY on a verdict the client library already
///        separated.
///
/// The wire's SHAPE — which flat fields make up which check — is owned by
/// `LibreSCRS::AgentClient::separateSecurityChecks()`, and nothing here reads a
/// field key. What is left is the part that could never live in a Qt-free
/// client library: turning the producer's open-string tokens into the closed
/// enumerations this pane paints and the printed record colours. Both surfaces
/// call this, so a token cannot come to mean two things depending on where the
/// read is rendered.
///
/// Every token is FOREIGN INPUT. One this build has not learned is not an
/// error: it is a verdict a newer agent is reporting correctly, and collapsing
/// it to the safest-LOOKING value would claim a check ran. An unrecognised
/// status therefore becomes @ref SecurityCheck::Status::NotPerformed — "nobody
/// here can say" — and an unrecognised category @ref SecurityCategory::Other.
///
/// @param verdict Any group piped through `separateSecurityChecks()`. A group
///        outside the verdict scope comes back with no checks and every field
///        in its aggregates, so this answers an empty model rather than
///        inventing one.
[[nodiscard]] SecurityStatusModel securityModelFrom(const LibreSCRS::AgentClient::SecurityVerdict& verdict);

} // namespace librecelik::utils

class SecurityStatusWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SecurityStatusWidget(QWidget* parent = nullptr);
    void setSecurityStatus(const librecelik::utils::SecurityStatusModel& status);

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildLayout();
    void retranslateUi();
    void refreshSummaryRows();
    void rebuildDetailRows();
    void applyDetailChecksState();
    QWidget* createStatusRow(const QString& label, librecelik::utils::SecurityCheck::Status status);
    QString statusColor(librecelik::utils::SecurityCheck::Status status) const;
    QString statusText(librecelik::utils::SecurityCheck::Status status) const;

    QVBoxLayout* mainLayout = nullptr;
    CollapsibleSection* section = nullptr;
    QLabel* integrityIcon = nullptr;
    QLabel* integrityLabel = nullptr;
    QLabel* authenticityIcon = nullptr;
    QLabel* authenticityLabel = nullptr;
    QLabel* genuinenessIcon = nullptr;
    QLabel* genuinenessLabel = nullptr;
    CollapsibleSection* detailSection = nullptr;
    // Cached for retranslate-on-language-change. hasStatus distinguishes
    // "never set" (initial NotPerformed display) from "real status applied".
    librecelik::utils::SecurityStatusModel cachedStatus;
    bool hasStatus = false;
};
