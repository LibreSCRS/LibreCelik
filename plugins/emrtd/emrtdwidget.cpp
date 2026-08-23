// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "emrtdwidget.h"

#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"
#include "utils/securitystatuswidget.h"

#include <QDate>
#include <QFont>
#include <QGridLayout>
#include <QStringList>

#include <algorithm>

#include <QHBoxLayout>
#include <QPainter>
#include <QToolButton>

#include <plugin/fieldvalue.h>

#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QVBoxLayout>

using librecelik::plugin::fieldDetailBytes;
using librecelik::plugin::findGroup;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

/// The portrait the gateway merged into the read: its own group, keyed with
/// the field half of the wire's composite key. A group that carries the image
/// under some other key still renders — the first field with bytes wins.
QByteArray photoBytes(const QList<FieldGroup>& groups)
{
    QByteArray bytes = fieldDetailBytes(groups, u"photo", u"photo");
    if (!bytes.isEmpty()) {
        return bytes;
    }
    if (const FieldGroup* group = findGroup(groups, u"photo")) {
        for (const Field& field : group->fields) {
            bytes = field.detail.toByteArray();
            if (!bytes.isEmpty()) {
                return bytes;
            }
        }
    }
    return {};
}

/// Bytes of a group's first field — the shape the single-image groups (DG5
/// portrait, DG7 signature) arrive in.
QByteArray firstFieldBytes(const FieldGroup& group)
{
    if (group.fields.isEmpty()) {
        return {};
    }
    return group.fields.first().detail.toByteArray();
}

// Translation maps as functions — qtTrId() must be called at runtime (not static init)
// to support runtime language switching.
std::map<QString, QString> documentTranslationMap()
{
    return {
        {QStringLiteral("document_number"), qtTrId("lc-emrtd-doc-number")},
        {QStringLiteral("document_code"), qtTrId("lc-emrtd-doc-code")},
        {QStringLiteral("issuing_state"), qtTrId("lc-emrtd-issuing-state")},
        {QStringLiteral("date_of_expiry"), qtTrId("lc-emrtd-date-of-expiry")},
        {QStringLiteral("personal_number"), qtTrId("lc-emrtd-personal-number")},
    };
}

std::map<QString, QString> documentExtraTranslationMap()
{
    return {
        {QStringLiteral("issuing_authority"), qtTrId("lc-emrtd-issuing-authority")},
        {QStringLiteral("date_of_issue"), qtTrId("lc-emrtd-date-of-issue")},
        {QStringLiteral("endorsements"), qtTrId("lc-emrtd-endorsements")},
        {QStringLiteral("tax_exit"), qtTrId("lc-emrtd-tax-exit")},
    };
}

std::map<QString, QString> personalTranslationMap()
{
    return {
        {QStringLiteral("given_names"), qtTrId("lc-emrtd-given-names")},
        {QStringLiteral("surname"), qtTrId("lc-emrtd-surname")},
        {QStringLiteral("nationality"), qtTrId("lc-emrtd-nationality")},
        {QStringLiteral("date_of_birth"), qtTrId("lc-emrtd-date-of-birth")},
        {QStringLiteral("sex"), qtTrId("lc-emrtd-sex")},
    };
}

std::map<QString, QString> additionalTranslationMap()
{
    return {
        {QStringLiteral("full_name"), qtTrId("lc-emrtd-full-name")},
        {QStringLiteral("other_names"), qtTrId("lc-emrtd-other-names")},
        {QStringLiteral("personal_number"), qtTrId("lc-emrtd-personal-number")},
        {QStringLiteral("place_of_birth"), qtTrId("lc-emrtd-place-of-birth")},
        {QStringLiteral("address"), qtTrId("lc-emrtd-address")},
        {QStringLiteral("telephone"), qtTrId("lc-emrtd-telephone")},
        {QStringLiteral("profession"), qtTrId("lc-emrtd-profession")},
        {QStringLiteral("title"), qtTrId("lc-emrtd-title")},
        {QStringLiteral("custody_info"), qtTrId("lc-emrtd-custody-info")},
    };
}

std::map<QString, QString> contactsTranslationMap()
{
    return {
        {QStringLiteral("name"), qtTrId("lc-emrtd-contact-name")},
        {QStringLiteral("telephone"), qtTrId("lc-emrtd-telephone")},
        {QStringLiteral("address"), qtTrId("lc-emrtd-address")},
    };
}

std::map<QString, QString> presenceTranslationMap()
{
    return {
        {QStringLiteral("data_groups"), qtTrId("lc-emrtd-data-groups")},
        {QStringLiteral("auth_method"), qtTrId("lc-emrtd-auth-method")},
    };
}

std::map<QString, QString> nationalTranslationMap()
{
    return {
        {QStringLiteral("tag"), qtTrId("lc-emrtd-national-tag")},
        {QStringLiteral("value"), qtTrId("lc-emrtd-national-value")},
    };
}

// --- the national annex -----------------------------------------------------
//
// A card may carry a signed annex of extra personal detail beside its
// travel-document data. It arrives as two groups whose keys are DERIVED from
// the annex's id — `annex.<id>.personal` and `annex.<id>.security` — so that a
// card carrying two annexes cannot have one silently shadow the other.

constexpr QLatin1StringView kAnnexPrefix{"annex."};

/// The `<id>` in `annex.<id>.<suffix>`, or empty when @p key is not an annex
/// key of exactly that shape.
///
/// Total by construction: the key is produced by the middleware, but this
/// widget treats it as foreign input, and half-parsing a malformed one into a
/// section with no id is worse than ignoring it.
QString annexIdOf(const QString& key, QLatin1StringView suffix)
{
    if (!key.startsWith(kAnnexPrefix)) {
        return {};
    }
    const QStringList parts = key.split(u'.');
    if (parts.size() != 3 || parts.at(1).isEmpty() || parts.at(2) != suffix) {
        return {};
    }
    return parts.at(1);
}

/// True for any annex group at all, well-formed or not — used only to stop a
/// malformed annex key falling through to another branch.
bool looksLikeAnnexKey(const QString& key)
{
    return key.startsWith(kAnnexPrefix);
}

/// Reading order for the annex's fields.
///
/// Identity crosses the wire as map-of-maps, so fields arrive sorted by KEY.
/// For a group whose substance is an address that means "Apartment" third and
/// "Street" last. This is the order a person reads an address in.
QStringList annexFieldOrder()
{
    return {
        QStringLiteral("address_label"),     QStringLiteral("street"),
        QStringLiteral("house_number"),      QStringLiteral("house_letter"),
        QStringLiteral("entrance"),          QStringLiteral("floor"),
        QStringLiteral("apartment_number"),  QStringLiteral("place"),
        QStringLiteral("community"),         QStringLiteral("state"),
        QStringLiteral("parent_given_name"), QStringLiteral("community_of_birth"),
        QStringLiteral("state_of_birth"),    QStringLiteral("document_serial"),
        QStringLiteral("address_date"),
    };
}

// The annex reader ships the address-change date as the card's raw ddMMyyyy
// digits (e.g. "06082016"); the middleware never reformats signed card bytes,
// so the display normalises it to dd.MM.yyyy here. A value that is already
// formatted, or that parses as neither shape (a placeholder), is left
// untouched — presentation-only, no data is invented.
LibreSCRS::AgentClient::FieldGroup normalizeAnnexDates(LibreSCRS::AgentClient::FieldGroup group)
{
    for (LibreSCRS::AgentClient::Field& field : group.fields) {
        if (field.key != QLatin1String("address_date") || field.value.isEmpty()) {
            continue;
        }
        if (QDate::fromString(field.value, QStringLiteral("dd.MM.yyyy")).isValid()) {
            continue; // already display-formatted
        }
        if (const QDate d = QDate::fromString(field.value, QStringLiteral("ddMMyyyy")); d.isValid()) {
            field.value = d.toString(QStringLiteral("dd.MM.yyyy"));
        }
    }
    return group;
}

std::map<QString, QString> annexTranslationMap()
{
    return {
        // Address, in reading order.
        {QStringLiteral("address_label"), qtTrId("lc-annex-address-label")},
        {QStringLiteral("street"), qtTrId("lc-annex-street")},
        {QStringLiteral("house_number"), qtTrId("lc-annex-house-number")},
        {QStringLiteral("house_letter"), qtTrId("lc-annex-house-letter")},
        {QStringLiteral("entrance"), qtTrId("lc-annex-entrance")},
        {QStringLiteral("floor"), qtTrId("lc-annex-floor")},
        {QStringLiteral("apartment_number"), qtTrId("lc-annex-apartment-number")},
        {QStringLiteral("place"), qtTrId("lc-annex-place")},
        {QStringLiteral("community"), qtTrId("lc-annex-community")},
        {QStringLiteral("state"), qtTrId("lc-annex-state")},
        // Origin and document.
        {QStringLiteral("parent_given_name"), qtTrId("lc-annex-parent-given-name")},
        {QStringLiteral("community_of_birth"), qtTrId("lc-annex-community-of-birth")},
        {QStringLiteral("state_of_birth"), qtTrId("lc-annex-state-of-birth")},
        {QStringLiteral("document_serial"), qtTrId("lc-annex-document-serial")},
        {QStringLiteral("address_date"), qtTrId("lc-annex-address-date")},
        // The verdict's own two fields.
        {QStringLiteral("annex_integrity"), qtTrId("lc-annex-integrity")},
        {QStringLiteral("annex_authenticity"), qtTrId("lc-annex-authenticity")},
    };
}

} // namespace

EMRTDWidget::EMRTDWidget(const QList<FieldGroup>& cardGroups, QWidget* parent) : EMRTDWidget(parent)
{
    // Staged into the widget's own section order — the final wire model's
    // group order is delivery-dependent (see stagedForBuild).
    for (const auto& group : librecelik::plugin::stagedForBuild(
             cardGroups,
             {u"personal", u"document", u"photo", u"signature", u"additional", u"document_extra", u"presence",
              u"portrait", u"contacts", u"biometric_fingerprint", u"biometric_iris", u"security_status"}))
        addGroup(group);
}

EMRTDWidget::EMRTDWidget(QWidget* parent) : plugin_ui::PluginWidgetBase(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    buildShell();
}

void EMRTDWidget::buildShell()
{
    // Navy outer CollapsibleSection — shell for the travel document display
    static const QColor navy(34, 86, 117);
    outerSection = new CollapsibleSection(qtTrId("lc-emrtd-travel-document"), navy, this);
    outerSection->setHeaderHeight(56);

    sectionLayout = new QVBoxLayout();
    sectionLayout->setSpacing(6);

    outerSection->setLayout(sectionLayout);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    outerLayout->addWidget(outerSection);

    // Print button — disabled (dimmed) until all streaming completes
    printBtn = iconutils::createPrinterHeaderButton(this);
    connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(groups); });
    outerSection->addHeaderWidget(printBtn);
}

void EMRTDWidget::addGroup(const FieldGroup& group)
{
    const QString& key = group.key;

    // Store the group for the fieldGroups() accessor
    groups.append(group);

    // Annex groups are matched on a PREFIX, never on the full key: the id in
    // the middle comes from the reader, and this issuer has already moved its
    // applet identifier once. Binding to `annex.rs.` would make the next annex
    // vanish exactly as both of these did before this branch existed.
    if (looksLikeAnnexKey(key)) {
        if (const QString id = annexIdOf(key, QLatin1StringView("personal")); !id.isEmpty()) {
            addAnnexPersonal(id, group);
        } else if (const QString secId = annexIdOf(key, QLatin1StringView("security")); !secId.isEmpty()) {
            addAnnexSecurity(secId, group);
        }
        // Anything else under the prefix is a key this build does not
        // understand: ignored, never guessed at.
        return;
    }

    if (key == QLatin1String("personal")) {
        auto* photoRow = new QHBoxLayout();
        photoRow->setSpacing(10);

        photoLabel = new QLabel(outerSection);
        QPixmap placeholder(190, 250);
        placeholder.fill(QColor(220, 220, 220));
        {
            QPixmap userIcon(QStringLiteral(":/images/user.png"));
            auto scaled = userIcon.scaled(QSize(95, 125), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPainter painter(&placeholder);
            painter.drawPixmap((190 - scaled.width()) / 2, (250 - scaled.height()) / 2, scaled);
        }
        photoLabel->setPixmap(placeholder);
        photoLabel->setFixedSize(190, 250);
        photoRow->addWidget(photoLabel, 0, Qt::AlignTop);

        auto* personalSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-personal-data-title"), group,
                                                                          personalTranslationMap(), {}, outerSection);
        personalSec->setCollapsible(false);
        photoRow->addWidget(personalSec, 1);

        sectionLayout->insertLayout(0, photoRow);
    } else if (key == QLatin1String("document")) {
        auto* docSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-document-data"), group,
                                                                         documentTranslationMap());
        librecelik::utils::FieldSectionBuilder::highlightExpiredDates(docSection, group,
                                                                      {QStringLiteral("date_of_expiry")});
        sectionLayout->addWidget(docSection);
    } else if (key == QLatin1String("photo")) {
        const QByteArray bytes = photoBytes(groups);
        if (!photoLabel || bytes.isEmpty())
            return;
        QPixmap photo;
        photo.loadFromData(bytes);
        if (!photo.isNull()) {
            auto scaledPhoto = photo.scaledToHeight(250, Qt::SmoothTransformation);
            photoLabel->setFixedSize(scaledPhoto.size());
            photoLabel->setPixmap(scaledPhoto);
        }
    } else if (key == QLatin1String("signature")) {
        const QByteArray bytes = firstFieldBytes(group);
        if (!bytes.isEmpty()) {
            auto* sigSection = new CollapsibleSection(qtTrId("lc-emrtd-signature"), outerSection);
            auto* sigLayout = new QVBoxLayout();
            auto* sigLabel = new QLabel();
            QPixmap sigPixmap;
            sigPixmap.loadFromData(bytes);
            sigLabel->setPixmap(sigPixmap.scaled(200, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sigLabel->setAlignment(Qt::AlignCenter);
            sigLayout->addWidget(sigLabel);
            sigSection->setLayout(sigLayout);
            sectionLayout->addWidget(sigSection);
        }
    } else if (key == QLatin1String("additional")) {
        auto* additionalSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-additional"), group,
                                                                                additionalTranslationMap());
        sectionLayout->addWidget(additionalSection);
    } else if (key == QLatin1String("document_extra")) {
        // If "additional" already added, show as separate "Issuing Information" section
        // Otherwise show as "Additional"
        bool hasAdditional = findGroup(groups, u"additional") != nullptr;
        if (hasAdditional) {
            auto* extraSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-issuing-info"), group,
                                                                               documentExtraTranslationMap());
            sectionLayout->addWidget(extraSection);
        } else {
            auto* extraSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-additional"), group,
                                                                               documentExtraTranslationMap());
            sectionLayout->addWidget(extraSection);
        }
    } else if (key == QLatin1String("presence")) {
        auto* presenceSection =
            librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-presence"), group, presenceTranslationMap());
        sectionLayout->addWidget(presenceSection);
    } else if (key == QLatin1String("portrait")) {
        // DG5 portrait image — similar to signature display
        const QByteArray bytes = firstFieldBytes(group);
        if (!bytes.isEmpty()) {
            auto* portraitSection = new CollapsibleSection(qtTrId("lc-emrtd-portrait"), outerSection);
            auto* portraitLayout = new QVBoxLayout();
            auto* portraitLabel = new QLabel();
            QPixmap portraitPixmap;
            portraitPixmap.loadFromData(bytes);
            if (!portraitPixmap.isNull()) {
                portraitLabel->setPixmap(
                    portraitPixmap.scaled(200, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            portraitLabel->setAlignment(Qt::AlignCenter);
            portraitLayout->addWidget(portraitLabel);
            portraitSection->setLayout(portraitLayout);
            sectionLayout->addWidget(portraitSection);
        }
    } else if (key == QLatin1String("contacts")) {
        auto* contactsSection =
            librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-contacts"), group, contactsTranslationMap());
        sectionLayout->addWidget(contactsSection);
    } else if (key == QLatin1String("biometric_fingerprint") || key == QLatin1String("biometric_iris")) {
        QString title = (key == QLatin1String("biometric_fingerprint")) ? qtTrId("lc-emrtd-biometric-fingerprint")
                                                                        : qtTrId("lc-emrtd-biometric-iris");
        auto* bioSection = new CollapsibleSection(title, outerSection);
        auto* bioLayout = new QVBoxLayout();
        auto* bioLabel = new QLabel(qtTrId("lc-emrtd-biometric-eac-required"));
        bioLabel->setStyleSheet("color: #E6873C; font-style: italic; padding: 8px;");
        bioLabel->setWordWrap(true);
        bioLayout->addWidget(bioLabel);
        bioSection->setLayout(bioLayout);
        sectionLayout->addWidget(bioSection);
    } else if (key == QLatin1String("security_status")) {
        // Parse fields back into the security model the pane renders
        using librecelik::utils::SecurityCategory;
        using librecelik::utils::SecurityCheck;
        librecelik::utils::SecurityStatusModel secStatus;
        for (const auto& field : group.fields) {
            const QString& text = field.value;
            if (field.key == QLatin1String("overall_integrity")) {
                secStatus.overallIntegrity =
                    librecelik::utils::statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
            } else if (field.key == QLatin1String("overall_authenticity")) {
                secStatus.overallAuthenticity =
                    librecelik::utils::statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
            } else if (field.key == QLatin1String("overall_genuineness")) {
                secStatus.overallGenuineness =
                    librecelik::utils::statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
            } else if (field.key.startsWith(QLatin1String("check_"))) {
                // Individual check fields: check_N_id, check_N_category, etc.
                // Parse grouped by index
                const qsizetype separator = field.key.indexOf(u'_', 6);
                if (separator < 0)
                    continue;
                const QString suffix = field.key.mid(separator + 1);
                const QString idxStr = field.key.mid(6, separator - 6);
                bool parsed = false;
                const uint idx = idxStr.toUInt(&parsed);
                if (!parsed)
                    continue;
                while (secStatus.checks.size() <= static_cast<qsizetype>(idx))
                    secStatus.checks.emplaceBack();
                auto& check = secStatus.checks[static_cast<qsizetype>(idx)];
                if (suffix == QLatin1String("id"))
                    check.checkId = text;
                else if (suffix == QLatin1String("category"))
                    check.category = librecelik::utils::categoryFromString(text).value_or(SecurityCategory::Other);
                else if (suffix == QLatin1String("status"))
                    check.status =
                        librecelik::utils::statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
                else if (suffix == QLatin1String("label"))
                    check.label = text;
                else if (suffix == QLatin1String("detail"))
                    check.detail = text;
                else if (suffix == QLatin1String("error"))
                    check.errorDetail = text;
            }
        }
        if (!securityStatusWidget) {
            securityStatusWidget = new SecurityStatusWidget(outerSection);
        }
        securityStatusWidget->setSecurityStatus(secStatus);
        securityStatusWidget->setVisible(true);
        // Always insert at top of section
        sectionLayout->insertWidget(0, securityStatusWidget);
    } else if (key == QLatin1String("national")) {
        auto* nationalSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-emrtd-national-data"), group,
                                                                              nationalTranslationMap());
        sectionLayout->addWidget(nationalSection);
    }
}

std::map<QString, QString> EMRTDWidget::annexTranslationMapForTest()
{
    return annexTranslationMap();
}

void EMRTDWidget::addAnnexPersonal(const QString& id, const FieldGroup& group)
{
    // One section per annex id. A repeat would overwrite the tracked pointer and
    // strand the first section in the layout: two identical headings on screen,
    // and any later verdict for this id reaching only the second. The key is
    // foreign input by this widget's own rule, so a duplicate is inside the
    // threat model rather than outside it.
    if (annexSections.contains(id)) {
        return;
    }

    auto* section =
        librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-annex-additional-data"), normalizeAnnexDates(group),
                                                      annexTranslationMap(), {}, outerSection, annexFieldOrder());
    annexSections.insert(id, section);
    sectionLayout->addWidget(section);

    // A verdict that arrived before its section did.
    if (const auto pending = pendingAnnexVerdicts.constFind(id); pending != pendingAnnexVerdicts.constEnd()) {
        const FieldGroup verdict = *pending;
        pendingAnnexVerdicts.erase(pending);
        addAnnexSecurity(id, verdict);
    }
}

void EMRTDWidget::addAnnexSecurity(const QString& id, const FieldGroup& group)
{
    CollapsibleSection* section = annexSections.value(id, nullptr);
    if (section == nullptr) {
        // The section is not there yet (streamed reads deliver emission order).
        // Hold it rather than rendering a verdict with no data behind it: on a
        // failed verification the middleware emits ZERO groups, so a lone
        // verdict never describes anything the reader can see.
        pendingAnnexVerdicts.insert(id, group);
        return;
    }

    auto* grid = qobject_cast<QGridLayout*>(section->layout());
    if (grid == nullptr) {
        return;
    }

    // Inside the section, spanning both columns. This placement IS the
    // requirement: the travel document's passive authentication and this
    // weaker verdict read as one guarantee when they share a flat list, and a
    // reader then credits the annex with a check nobody ran.
    // Bound to a local ONCE. Calling the factory twice in one expression builds
    // two independent maps, so the iterator and the end() it is compared
    // against belong to different containers — and both temporaries die at the
    // semicolon, before the value is read.
    const std::map<QString, QString> labels = annexTranslationMap();

    // An empty group would leave the heading below describing nothing.
    if (group.fields.isEmpty()) {
        return;
    }

    int row = grid->rowCount();

    // A scoped sub-heading so the badges below read as a verdict ABOUT the
    // annex — parallel to the travel document's own "Travel Document
    // Verification" title — rather than two more data rows the reader might
    // credit to the passport. Bold, spanning both columns, with breathing room
    // above.
    auto* verdictHeading = new QLabel(qtTrId("lc-annex-verification"), section);
    verdictHeading->setObjectName(QStringLiteral("annexVerdictHeading"));
    QFont headingFont = verdictHeading->font();
    headingFont.setBold(true);
    verdictHeading->setFont(headingFont);
    verdictHeading->setContentsMargins(0, 8, 0, 0);
    grid->addWidget(verdictHeading, row++, 0, 1, 2);

    const auto renderRow = [&](const Field& field) {
        const auto status = librecelik::utils::statusFromString(field.value)
                                .value_or(librecelik::utils::SecurityCheck::Status::NotPerformed);
        const QString fallback = field.extra.value(QStringLiteral("labelFallback")).toString();
        const auto label = labels.find(field.key);
        const QString text = label != labels.end() ? label->second : (fallback.isEmpty() ? field.key : fallback);
        grid->addWidget(librecelik::utils::makeStatusRow(text, status, section), row++, 0, 1, 2);
    };

    // Fixed order — integrity then authenticity — matching the travel
    // document's pane. The wire delivers the verdict as a key-sorted map, so
    // "annex_authenticity" would otherwise sort ahead of "annex_integrity" and
    // the two panes would disagree on order.
    const QStringList pinned{QStringLiteral("annex_integrity"), QStringLiteral("annex_authenticity")};
    for (const QString& key : pinned) {
        const auto it =
            std::find_if(group.fields.begin(), group.fields.end(), [&key](const Field& f) { return f.key == key; });
        if (it != group.fields.end()) {
            renderRow(*it);
        }
    }
    // Every other field the wire ships in this group still renders — in
    // delivery order, through the same labelFallback path — so the shared
    // vocabulary can grow without this widget silently dropping rows.
    for (const Field& field : group.fields) {
        if (!pinned.contains(field.key)) {
            renderRow(field);
        }
    }
}

void EMRTDWidget::showNoDataMessage()
{
    noDataMessageShown = true;
    auto* msgLabel = new QLabel(qtTrId("lc-emrtd-no-data-message"), outerSection);
    msgLabel->setObjectName(QStringLiteral("emrtdNoDataMessage"));
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("color: #CC3333; font-size: 13px; padding: 12px;");
    sectionLayout->addWidget(msgLabel);
}

void EMRTDWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

void EMRTDWidget::retranslateUi()
{
    // Plugin widget rebuild-tier (April 2026 retranslate spec): tear
    // down the shell and rebuild from the cached groups so every label
    // produced via translation maps refreshes with the new translator.
    auto cachedGroups = std::move(groups);
    groups.clear();
    const bool hadNoData = noDataMessageShown;
    noDataMessageShown = false;

    if (outerSection) {
        outerLayout->removeWidget(outerSection);
        outerSection->deleteLater();
        outerSection = nullptr;
    }
    sectionLayout = nullptr;
    photoLabel = nullptr;
    securityStatusWidget = nullptr;
    printBtn = nullptr;
    // The sections were children of the torn-down shell; the pending map has to
    // go with them, or a verdict already rendered would be replayed onto the
    // rebuilt section a second time.
    annexSections.clear();
    pendingAnnexVerdicts.clear();

    buildShell();
    for (const auto& group : cachedGroups)
        addGroup(group);
    if (hadNoData)
        showNoDataMessage();
}
