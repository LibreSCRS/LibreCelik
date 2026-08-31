// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "securitystatuswidget.h"

#include "utils/collapsiblesection.h"

#include <QEvent>
#include <QGlobalStatic>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1StringView>
#include <QVBoxLayout>

#include <utility>

namespace librecelik::utils {

QString localizedStatusText(SecurityCheck::Status status)
{
    switch (status) {
    case SecurityCheck::Status::Passed:
        return qtTrId("lc-emrtd-security-passed");
    case SecurityCheck::Status::Failed:
        return qtTrId("lc-emrtd-security-failed");
    case SecurityCheck::Status::NotSupported:
        return qtTrId("lc-emrtd-security-not-supported");
    case SecurityCheck::Status::Skipped:
        return qtTrId("lc-emrtd-security-skipped");
    case SecurityCheck::Status::NotPerformed:
        return qtTrId("lc-emrtd-security-not-performed");
    }
    return qtTrId("lc-emrtd-security-not-performed");
}

QString localizedReasonText(const QString& reasonKey)
{
    // A named key first, so a build that knows the reason always speaks the
    // holder's language. Four of the five below are states of THIS
    // installation's own configuration and arrive as NOT_PERFORMED, not as
    // FAILED: nothing was proven about the document, so each says what to fix
    // rather than what the document is. Only the last one is about the
    // document.
    if (reasonKey == QLatin1StringView("csca.not-configured"))
        return qtTrId("lc-emrtd-csca-not-configured");
    if (reasonKey == QLatin1StringView("csca.anchors-unreadable"))
        return qtTrId("lc-emrtd-csca-anchors-unreadable");
    if (reasonKey == QLatin1StringView("csca.anchors-undecodable"))
        return qtTrId("lc-emrtd-csca-anchors-undecodable");
    if (reasonKey == QLatin1StringView("csca.no-anchor-for-issuer"))
        return qtTrId("lc-emrtd-csca-no-anchor-for-issuer");
    if (reasonKey == QLatin1StringView("csca.chain-failed"))
        return qtTrId("lc-emrtd-csca-chain-failed");
    // Append-only vocabulary: an unnamed key declines every arm above and
    // falls through as itself. Nothing asserts, and the row keeps its line.
    return reasonKey;
}

QString statusColorHex(SecurityCheck::Status status)
{
    switch (status) {
    case SecurityCheck::Status::Passed:
        return QStringLiteral("#4CAF50");
    case SecurityCheck::Status::Failed:
        return QStringLiteral("#F44336");
    case SecurityCheck::Status::NotSupported:
    case SecurityCheck::Status::Skipped:
        return QStringLiteral("#FFC107");
    case SecurityCheck::Status::NotPerformed:
        return QStringLiteral("#9E9E9E");
    }
    return QStringLiteral("#9E9E9E");
}

SecurityStatusModel securityModelFrom(const LibreSCRS::AgentClient::SecurityVerdict& verdict)
{
    SecurityStatusModel model;

    // The three roll-ups arrive already aggregated and travel through the
    // library untouched, as ordinary carried-over fields. Nothing here
    // recomputes one from the checks: a pane that derived "authenticity" from
    // the checks it happened to recognise would disagree with the agent about
    // the document the moment a newer check appeared.
    for (const LibreSCRS::AgentClient::Field& field : verdict.aggregates) {
        if (field.key == QLatin1StringView("overall_integrity")) {
            model.overallIntegrity = statusFromString(field.value).value_or(SecurityCheck::Status::NotPerformed);
        } else if (field.key == QLatin1StringView("overall_authenticity")) {
            model.overallAuthenticity = statusFromString(field.value).value_or(SecurityCheck::Status::NotPerformed);
        } else if (field.key == QLatin1StringView("overall_genuineness")) {
            model.overallGenuineness = statusFromString(field.value).value_or(SecurityCheck::Status::NotPerformed);
        }
        // Anything else the group carried is not this pane's to render. It is
        // dropped rather than guessed at -- an annex verdict's own two fields
        // reach a different surface, and a key from a later agent has no row
        // here that would mean anything.
    }

    model.checks.reserve(verdict.checks.size());
    for (const LibreSCRS::AgentClient::SecurityCheckEntry& entry : verdict.checks) {
        SecurityCheck check;
        check.checkId = entry.id;
        check.category = categoryFromString(entry.category).value_or(SecurityCategory::Other);
        check.status = statusFromString(entry.status).value_or(SecurityCheck::Status::NotPerformed);
        check.label = entry.label;
        check.detail = entry.detail;
        check.errorDetail = entry.error;
        // A KEY, carried as it arrived. localizedReasonText() is the only
        // thing that may turn it into words.
        check.reason = entry.reason;
        model.checks.append(std::move(check));
    }
    return model;
}

namespace {

/// The reader's own choice for the per-check block, for the life of the process
/// and no longer. Application-scope state in a Qt host, per this project's
/// singleton policy.
Q_GLOBAL_STATIC(std::optional<bool>, g_detailChecksChoice)

} // namespace

bool detailChecksExpandedFor(const SecurityStatusModel& status)
{
    // Two outcomes open the block, and they are named rather than derived from
    // "not Passed": NOT_SUPPORTED and SKIPPED say something about the card or
    // about this read, not about something the holder can fix, and treating
    // them as trouble would leave the block open on ordinary documents until it
    // stopped meaning anything.
    for (const SecurityCheck& check : status.checks) {
        if (check.status == SecurityCheck::Status::Failed || check.status == SecurityCheck::Status::NotPerformed) {
            return true;
        }
    }
    return false;
}

std::optional<bool> rememberedDetailChecksChoice()
{
    // Q_GLOBAL_STATIC hands back a null pointer once the destructor has run at
    // shutdown; a pane torn down that late gets the derived default rather than
    // a dereferenced corpse.
    return g_detailChecksChoice.exists() ? *g_detailChecksChoice : std::nullopt;
}

void rememberDetailChecksChoice(bool expanded)
{
    // exists() would be wrong here: it is false BEFORE the first write as well
    // as after teardown, and the first write is the one that matters.
    if (g_detailChecksChoice.isDestroyed()) {
        return;
    }
    *g_detailChecksChoice = expanded;
}

void forgetDetailChecksChoice()
{
    if (g_detailChecksChoice.exists()) {
        g_detailChecksChoice->reset();
    }
}

QWidget* makeStatusRow(const QString& label, SecurityCheck::Status status, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(4, 2, 4, 2);
    rowLayout->setSpacing(8);

    auto* icon = new QLabel(row);
    icon->setObjectName(QStringLiteral("icon"));
    icon->setFixedSize(16, 16);
    icon->setStyleSheet(QStringLiteral("background: %1; border-radius: 8px;").arg(statusColorHex(status)));
    icon->setAccessibleName(localizedStatusText(status));

    const QString text = label + QStringLiteral(": ") + localizedStatusText(status);
    auto* textLabel = new QLabel(text, row);
    textLabel->setObjectName(QStringLiteral("text"));
    // Card-derived vocabulary reaches this row; never let AutoText promote a
    // value that happens to look like markup.
    textLabel->setTextFormat(Qt::PlainText);
    textLabel->setStyleSheet(QStringLiteral("font-size: 12px;"));
    textLabel->setAccessibleName(text);

    rowLayout->addWidget(icon);
    rowLayout->addWidget(textLabel, 1);
    return row;
}

} // namespace librecelik::utils

SecurityStatusWidget::SecurityStatusWidget(QWidget* parent) : QWidget(parent)
{
    buildLayout();
    retranslateUi();
}

void SecurityStatusWidget::buildLayout()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Section title is set by retranslateUi() so that LanguageChange
    // paths are the single source of truth.
    section = new CollapsibleSection(QString(), this);
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(6);

    // Three summary rows — created with empty labels; populated by
    // refreshSummaryRows() (called from retranslateUi()).
    auto* integrityRow = createStatusRow(QString(), librecelik::utils::SecurityCheck::Status::NotPerformed);
    integrityIcon = integrityRow->findChildren<QLabel*>("icon").value(0);
    integrityLabel = integrityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(integrityRow);

    auto* authenticityRow = createStatusRow(QString(), librecelik::utils::SecurityCheck::Status::NotPerformed);
    authenticityIcon = authenticityRow->findChildren<QLabel*>("icon").value(0);
    authenticityLabel = authenticityRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(authenticityRow);

    auto* genuinenessRow = createStatusRow(QString(), librecelik::utils::SecurityCheck::Status::NotPerformed);
    genuinenessIcon = genuinenessRow->findChildren<QLabel*>("icon").value(0);
    genuinenessLabel = genuinenessRow->findChildren<QLabel*>("text").value(0);
    contentLayout->addWidget(genuinenessRow);

    // The per-check block — a section of its own, built with the same component
    // every neighbouring block in this pane uses, so there is exactly one thing
    // on the pane that knows how to collapse. It sits BELOW the three roll-ups
    // and contains none of them: closing it must never take a verdict with it.
    // Title comes from retranslateUi(), like the section around it.
    detailSection = new CollapsibleSection(QString(), section);
    // Nested two deep inside the pane; the animation only buys a bounce here,
    // and an instant toggle is what the neighbouring nested sections do.
    detailSection->setAnimated(false);
    detailSection->setVisible(false);
    contentLayout->addWidget(detailSection);
    connect(detailSection, &CollapsibleSection::toggledByUser, this,
            [](bool expanded) { librecelik::utils::rememberDetailChecksChoice(expanded); });

    section->setLayout(contentLayout);
    mainLayout->addWidget(section);
}

void SecurityStatusWidget::refreshSummaryRows()
{
    // Update icon colour and label text from cachedStatus (or initial
    // NotPerformed when no status has been applied yet). Reads label
    // text from qtTrId at call time so language-change repopulates.
    auto updateRow = [this](QLabel* icon, QLabel* text, const QString& label,
                            librecelik::utils::SecurityCheck::Status s) {
        if (icon)
            icon->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(statusColor(s)));
        if (text)
            text->setText(label + ": " + statusText(s));
    };
    using Status = librecelik::utils::SecurityCheck::Status;
    const Status integ = hasStatus ? cachedStatus.overallIntegrity : Status::NotPerformed;
    const Status auth = hasStatus ? cachedStatus.overallAuthenticity : Status::NotPerformed;
    const Status genu = hasStatus ? cachedStatus.overallGenuineness : Status::NotPerformed;
    updateRow(integrityIcon, integrityLabel, qtTrId("lc-emrtd-security-integrity"), integ);
    updateRow(authenticityIcon, authenticityLabel, qtTrId("lc-emrtd-security-authenticity"), auth);
    updateRow(genuinenessIcon, genuinenessLabel, qtTrId("lc-emrtd-security-genuineness"), genu);
}

QWidget* SecurityStatusWidget::createStatusRow(const QString& label, librecelik::utils::SecurityCheck::Status status)
{
    // Delegates to the shared builder rather than keeping a second copy of the
    // same row. The copy that used to live here also lacked the PlainText
    // format the shared one sets, so a check label shaped like markup was
    // rendered as markup in this pane only.
    return librecelik::utils::makeStatusRow(label, status);
}

QString SecurityStatusWidget::statusColor(librecelik::utils::SecurityCheck::Status status) const
{
    return librecelik::utils::statusColorHex(status);
}

QString SecurityStatusWidget::statusText(librecelik::utils::SecurityCheck::Status status) const
{
    return librecelik::utils::localizedStatusText(status);
}

void SecurityStatusWidget::setSecurityStatus(const librecelik::utils::SecurityStatusModel& status)
{
    // Cache for retranslate-on-language-change (per LM 4.0 retranslate
    // pattern: rebuild dynamic content from cached state).
    cachedStatus = status;
    hasStatus = true;

    refreshSummaryRows();
    rebuildDetailRows();
}

void SecurityStatusWidget::rebuildDetailRows()
{
    // Build detail section with individual checks. Called both from
    // setSecurityStatus() (when fresh status arrives) and from
    // retranslateUi() (so the "Details" header re-renders in the new
    // language). check.label / check.detail are strings the read itself
    // supplied and are not retranslated here.
    // Clearing the block has to clear the ROWS, not just the layout arranging
    // them. Deleting a QLayout never deletes the widgets it managed, and each
    // check row is a NESTED layout — so takeAt()'s item->widget() is null for
    // it and a loop over the items walked straight past that row's icon and
    // label. They stayed behind as orphaned children, one full set per rebuild,
    // drawn at whatever geometry they last had. Deleting the direct children
    // deletes the rows themselves, whatever layout was arranging them.
    qDeleteAll(detailSection->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));
    delete detailSection->layout();

    if (!hasStatus || cachedStatus.checks.isEmpty()) {
        detailSection->setVisible(false);
        return;
    }

    auto* detailLayout = new QVBoxLayout(detailSection);
    detailLayout->setContentsMargins(8, 4, 4, 4);
    detailLayout->setSpacing(2);

    // No heading label inside: the section paints its own title in the header
    // bar, which is the half that stays on screen when the block is closed. A
    // second copy in the content would say the same thing twice while open and
    // nothing at all while closed.

    for (const auto& check : cachedStatus.checks) {
        auto* checkRow = new QHBoxLayout();
        checkRow->setSpacing(6);

        auto* checkIcon = new QLabel();
        checkIcon->setFixedSize(10, 10);
        checkIcon->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(statusColor(check.status)));

        auto* checkLabel = new QLabel(check.label);
        checkLabel->setObjectName(QStringLiteral("checkLabel"));
        checkLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(palette().color(QPalette::Text).name()));
        checkLabel->setWordWrap(true);

        checkRow->addWidget(checkIcon);
        checkRow->addWidget(checkLabel, 1);
        detailLayout->addLayout(checkRow);

        // Above the read's own detail line, and in the ordinary text colour
        // rather than the placeholder grey: this is the line that tells the
        // holder what to DO, and it is the only line on the pane that does.
        const QString reason = librecelik::utils::localizedReasonText(check.reason);
        if (!reason.isEmpty()) {
            auto* reasonText = new QLabel(reason);
            reasonText->setObjectName(QStringLiteral("reasonText"));
            // An unnamed key reaches the holder verbatim, so a reason shaped
            // like markup must render as the token it is.
            reasonText->setTextFormat(Qt::PlainText);
            reasonText->setStyleSheet(
                QString("font-size: 11px; color: %1; margin-left: 16px;").arg(palette().color(QPalette::Text).name()));
            reasonText->setWordWrap(true);
            detailLayout->addWidget(reasonText);
        }

        if (!check.detail.isEmpty()) {
            auto* detailText = new QLabel(check.detail);
            detailText->setObjectName(QStringLiteral("detailText"));
            detailText->setStyleSheet(QString("font-size: 10px; color: %1; margin-left: 16px;")
                                          .arg(palette().color(QPalette::PlaceholderText).name()));
            detailText->setWordWrap(true);
            detailLayout->addWidget(detailText);
        }
    }

    detailSection->setVisible(true);
    applyDetailChecksState();
}

void SecurityStatusWidget::applyDetailChecksState()
{
    // The reader's own choice outranks the derived default, and outranks it for
    // the rest of the session: re-deciding on the next card is the behaviour
    // this guard exists to prevent.
    const bool expand = librecelik::utils::rememberedDetailChecksChoice().value_or(
        librecelik::utils::detailChecksExpandedFor(cachedStatus));
    detailSection->setExpanded(expand);
    // setExpanded() returns early when the flag already matches, and the rows
    // above are NEW children the layout has just made visible. Without this the
    // second verdict of a streamed read fills a block the reader had closed.
    detailSection->refreshContentVisibility();
}

void SecurityStatusWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (event->type() == QEvent::PaletteChange) {
        // Deliberately not gated on isVisible(): the rows of a CLOSED block are
        // hidden, and a palette that changed while it was closed has to have
        // reached them by the time the reader opens it again.
        if (detailSection) {
            const QString textColor = palette().color(QPalette::Text).name();
            const QString placeholderColor = palette().color(QPalette::PlaceholderText).name();

            for (auto* label : detailSection->findChildren<QLabel*>(QStringLiteral("checkLabel")))
                label->setStyleSheet(QString("font-size: 11px; color: %1;").arg(textColor));
            for (auto* label : detailSection->findChildren<QLabel*>(QStringLiteral("reasonText")))
                label->setStyleSheet(QString("font-size: 11px; color: %1; margin-left: 16px;").arg(textColor));
            for (auto* label : detailSection->findChildren<QLabel*>(QStringLiteral("detailText")))
                label->setStyleSheet(QString("font-size: 10px; color: %1; margin-left: 16px;").arg(placeholderColor));
        }
    }
    QWidget::changeEvent(event);
}

void SecurityStatusWidget::retranslateUi()
{
    section->setTitle(qtTrId("lc-emrtd-security-status-travel-doc"));
    detailSection->setTitle(qtTrId("lc-emrtd-security-details"));
    refreshSummaryRows();
    rebuildDetailRows();
}
