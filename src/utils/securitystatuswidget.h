// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

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

/// @brief Category of a @ref SecurityCheck.
///
/// The security pane is fed by a card read whose checks arrive as wire fields
/// — a closed vocabulary carried as strings. This is the host-side
/// re-declaration of that vocabulary, so the widget owns the enumeration it
/// renders instead of borrowing one from a card-model library it no longer
/// speaks.
enum class SecurityCategory : std::uint8_t {
    DataIntegrity, ///< Hash/MAC integrity of data groups.
    Authenticity,  ///< Signature / passive authentication over the data.
    Genuineness,   ///< Chip genuineness (active authentication / chip auth).
    Other,         ///< A check outside the canonical categories.
};

/// @brief One security-verification check result (e.g. passive auth, chip auth).
struct SecurityCheck
{
    /// @brief Outcome of a single security check.
    ///
    /// The string form preserved by @ref statusToString / @ref statusFromString
    /// is the wire's UPPERCASE token ("PASSED", "FAILED", "NOT_PERFORMED",
    /// "NOT_SUPPORTED", "SKIPPED").
    enum class Status : std::uint8_t {
        Passed,       ///< Check was performed and succeeded.
        Failed,       ///< Check was performed and failed.
        NotPerformed, ///< Check was not run (e.g. prerequisite missing).
        NotSupported, ///< Card does not implement the check.
        Skipped,      ///< The read chose to bypass the check.
    };

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

/// @brief Canonical string form of a status.
[[nodiscard]] inline QString statusToString(SecurityCheck::Status s)
{
    switch (s) {
    case SecurityCheck::Status::Passed:
        return QStringLiteral("PASSED");
    case SecurityCheck::Status::Failed:
        return QStringLiteral("FAILED");
    case SecurityCheck::Status::NotPerformed:
        return QStringLiteral("NOT_PERFORMED");
    case SecurityCheck::Status::NotSupported:
        return QStringLiteral("NOT_SUPPORTED");
    case SecurityCheck::Status::Skipped:
        return QStringLiteral("SKIPPED");
    }
    return QStringLiteral("UNKNOWN");
}

/// @brief Parse a canonical status string; @c std::nullopt for unrecognized
///        or empty input (the caller decides what an unknown security verdict
///        means — collapsing it to NotPerformed is the safest-looking wrong
///        answer).
[[nodiscard]] inline std::optional<SecurityCheck::Status> statusFromString(QStringView s)
{
    if (s == u"PASSED")
        return SecurityCheck::Status::Passed;
    if (s == u"FAILED")
        return SecurityCheck::Status::Failed;
    if (s == u"NOT_PERFORMED")
        return SecurityCheck::Status::NotPerformed;
    if (s == u"NOT_SUPPORTED")
        return SecurityCheck::Status::NotSupported;
    if (s == u"SKIPPED")
        return SecurityCheck::Status::Skipped;
    return std::nullopt;
}

/// @brief Canonical string form of a category.
[[nodiscard]] inline QString categoryToString(SecurityCategory c)
{
    switch (c) {
    case SecurityCategory::DataIntegrity:
        return QStringLiteral("data_integrity");
    case SecurityCategory::Authenticity:
        return QStringLiteral("data_authenticity");
    case SecurityCategory::Genuineness:
        return QStringLiteral("chip_genuineness");
    case SecurityCategory::Other:
        return QStringLiteral("other");
    }
    return QStringLiteral("unknown");
}

/// @brief Parse a category token; @c std::nullopt for unknown input.
[[nodiscard]] inline std::optional<SecurityCategory> categoryFromString(QStringView s)
{
    if (s == u"data_integrity")
        return SecurityCategory::DataIntegrity;
    if (s == u"data_authenticity")
        return SecurityCategory::Authenticity;
    if (s == u"chip_genuineness")
        return SecurityCategory::Genuineness;
    if (s == u"other")
        return SecurityCategory::Other;
    return std::nullopt;
}

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
    QWidget* detailWidget = nullptr;
    // Cached for retranslate-on-language-change. hasStatus distinguishes
    // "never set" (initial NotPerformed display) from "real status applied".
    librecelik::utils::SecurityStatusModel cachedStatus;
    bool hasStatus = false;
};
