// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "settingsdialog.h"

#include "agent/agentgateway.h"
#include "agent/settingsimport.h"
#include "settings/settingskeys.h"
#include "settings/tlitemdelegate.h"
#include "signing/tsaitemdelegate.h"
#include "utils/dialogs.h"
#include "utils/buttonbox.h"
#include "utils/localeresolver.h"

#include <LibreSCRS/AgentClient/FdHandle.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QInputDialog>
#include <QLocale>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPalette>
#include <QPair>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include <fcntl.h>

#include <utility>

namespace {

using librecelik::agent::PresenceState;

// Config1 keys, in the wire's own spelling. These are the agent's names, not
// the QSettings paths the same preferences used to live under.
constexpr QLatin1String kDefaultLevel{"DefaultLevel"};
constexpr QLatin1String kDefaultReason{"DefaultReason"};
constexpr QLatin1String kDefaultLocation{"DefaultLocation"};
constexpr QLatin1String kTsaUrls{"TsaUrls"};
constexpr QLatin1String kLastTsaUrl{"LastTsaUrl"};
constexpr QLatin1String kTslSources{"TslSources"};
constexpr QLatin1String kCscaAnchorState{"CscaAnchorState"};

// The members of the `CscaAnchorState` dictionary, in the wire's own spelling.
// This dialog reads the DICTIONARY rather than the client's value struct
// because an optional member the agent did not send arrives as an ABSENT KEY,
// and the struct would zero it — which is the difference between "the accepted
// list carried no signing time" and "the list was signed on 1970-01-01".
constexpr QLatin1String kCscaAnchors{"anchors"};
constexpr QLatin1String kCscaIssuers{"issuers"};
constexpr QLatin1String kCscaReplayRefusalActive{"replayRefusalActive"};
constexpr QLatin1String kCscaSignedAt{"signedAt"};

// The public download portal for eMRTD country-signing master lists. Verified
// to serve "the latest collection of eMRTD CSCA ML" behind a terms-and-
// conditions page, with no login and no directory membership — which is why it
// can be named here, and why nothing downloads it automatically: a person has
// to accept those terms. It is the ONLY address this screen names. Other
// issuers publish lists of their own, but a wrong address in a dialog spends a
// reader's time and their trust in everything else the dialog says.
constexpr QLatin1String kIcaoPkdUrl{"https://download.pkd.icao.int/"};

// The tabs whose content the agent owns; the General tab is this process's own
// and stays usable with no agent at all.
constexpr int kSigningTabIndex = 1;
constexpr int kTrustTabIndex = 2;

// How tall a settings list box is allowed to be, in rows. Both of them size to
// what they hold rather than swallowing their tab: a box five times taller than
// its one entry reads as a list that failed to load. The FLOOR keeps an empty
// one looking like somewhere entries go; past the CEILING it scrolls, which is
// the only honest thing a box can do with more rows than fit.
constexpr int kListMinRows = 3;
constexpr int kListMaxRows = 6;

/// Fix @p list to the height of the rows it actually holds, within those
/// bounds.
///
/// The delegate decides how tall a row is, so ASK it rather than multiplying a
/// font height: the "add" sentinel is shorter than a configured entry, which
/// carries badges, and a floor measured off the sentinel clips the entries. The
/// TALLEST row is what a row-count bound has to be counted in.
void sizeListToContents(QListWidget* list)
{
    const int rows = list->count();
    const int chrome = 2 * list->frameWidth();
    int rowHeight = list->fontMetrics().height() + 8;
    int content = chrome;
    for (int row = 0; row < rows; ++row) {
        const int height = list->sizeHintForRow(row);
        content += height;
        rowHeight = qMax(rowHeight, height);
    }
    list->setFixedHeight(qBound(chrome + kListMinRows * rowHeight, content, chrome + kListMaxRows * rowHeight));
}

/// The application's own voice for text that is there to be scanned past: one
/// point down, and the palette's placeholder colour. The same two moves the
/// signing wizard makes on its file-size line, so a reader meets ONE quiet
/// treatment across the application rather than a fresh invention per screen.
/// A palette ROLE rather than a colour literal, so it follows a theme change
/// without this dialog having to hear about it.
void makeSecondary(QLabel* label)
{
    QFont font = label->font();
    const qreal points = font.pointSizeF();
    if (points > 1.0)
        font.setPointSizeF(points - 1.0);
    label->setFont(font);
    label->setForegroundRole(QPalette::PlaceholderText);
}

/// A horizontal rule, in the shape the about dialog already draws one.
QFrame* makeSeparator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

/// The signing tab's keys, in write order: the settings-tier ones first, so a
/// refused trust-tier write cannot cost the human the edits that needed no
/// authorisation.
const QStringList& signingKeys()
{
    static const QStringList keys = {kDefaultLevel, kDefaultReason, kDefaultLocation, kTsaUrls};
    return keys;
}

const QStringList& trustKeys()
{
    static const QStringList keys = {kTslSources};
    return keys;
}

bool isValidServiceUrl(const QString& url)
{
    QUrl parsed(url);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return false;
    const QString scheme = parsed.scheme();
    return scheme == QStringLiteral("https") || scheme == QStringLiteral("http");
}

/// The combo carries the UI tokens the level widgets have always used; the
/// agent speaks the wire's own. The two are translated at the seam rather than
/// respelled through the whole GUI, so an unknown token from either side falls
/// back to the baseline level instead of selecting nothing.
QString wireLevelToken(const QString& uiToken)
{
    if (uiToken == QStringLiteral("B_T"))
        return QStringLiteral("b-t");
    if (uiToken == QStringLiteral("B_LT"))
        return QStringLiteral("b-lt");
    if (uiToken == QStringLiteral("B_LTA"))
        return QStringLiteral("b-lta");
    return QStringLiteral("b-b");
}

QString uiLevelToken(const QString& wireToken)
{
    if (wireToken == QStringLiteral("b-t"))
        return QStringLiteral("B_T");
    if (wireToken == QStringLiteral("b-lt"))
        return QStringLiteral("B_LT");
    if (wireToken == QStringLiteral("b-lta"))
        return QStringLiteral("B_LTA");
    return QStringLiteral("B_B");
}

/// The reply an accepted import answered with, spelled into the same shape the
/// agent's read-only property carries — so the summary has exactly ONE input
/// to render and cannot come to say two different things depending on how the
/// state was learned.
///
/// The absent-never-zero rule survives the crossing: an invalid QDateTime
/// becomes a MISSING key, never an epoch-valued stand-in. The three members
/// above it are always written, so an accepted import can never produce the
/// empty map that means "nothing installed".
QVariantMap anchorStateAsMap(const LibreSCRS::AgentClient::CscaAnchorState& state)
{
    QVariantMap map;
    map.insert(kCscaAnchors, state.anchors);
    map.insert(kCscaIssuers, state.issuers);
    map.insert(kCscaReplayRefusalActive, state.replayRefusalActive);
    if (state.signedAt.isValid()) {
        map.insert(kCscaSignedAt, state.signedAt.toSecsSinceEpoch());
    }
    return map;
}

} // namespace

SettingsDialog::SettingsDialog(librecelik::agent::AgentGateway* agentGateway, QWidget* parent)
    : QDialog(parent), gateway(agentGateway)
{
    // The floor is set by the tallest tab, which is Trust: two framed sections,
    // and an anchor section that has to fit its account of what is installed
    // AND the sentences telling a reader where a master list comes from.
    //
    // 560 is MEASURED, not chosen: below it the anchor summary and the download
    // sentence lose their last lines, and they do so in every catalogue — the
    // Serbian text is the longest and reaches the same floor at any width this
    // dialog can be narrowed to. The minimum shipped before this was 520x520,
    // which no catalogue could render: at that size a third of the download
    // paragraph was cut off in all three anchor states. A minimum a dialog
    // cannot draw at is not a minimum.
    //
    // The default is wider than the floor rather than taller, because width is
    // what this tab's prose actually wants: past 680 the paragraphs fall into
    // fewer lines and the tab needs 30px less height than it does at 640.
    setMinimumSize(520, 560);
    resize(680, 560);

    auto* layout = new QVBoxLayout(this);

    tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    // --- General tab ---
    auto* generalTab = new QWidget(this);
    auto* generalLayout = new QFormLayout(generalTab);

    languageCombo = new QComboBox(generalTab);
    languageCombo->setObjectName(QStringLiteral("languageCombo"));
    languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageCombo->addItem(QStringLiteral("Српски"), QStringLiteral("sr_RS"));
    // Latin-script rendering of the same Serbian catalogue. Cyrillic stays
    // the primary Serbian entry above; this is an explicit additional
    // choice, never a silent replacement.
    languageCombo->addItem(QStringLiteral("Srpski (latinica)"), QStringLiteral("sr_Latn_RS"));
    languageLabel = new QLabel(generalTab);
    generalLayout->addRow(languageLabel, languageCombo);

    connect(languageCombo, &QComboBox::currentIndexChanged, this, [this]() {
        // Live preview — emit immediately but don't persist until OK is clicked
        emit languageChanged(languageCombo->currentData().toString());
    });

    tabs->addTab(generalTab, QString());

    // --- Signing tab ---
    auto* signingTab = new QWidget(this);
    auto* signingLayout = new QVBoxLayout(signingTab);

    auto* signingForm = new QFormLayout;

    defaultLevelCombo = new QComboBox(signingTab);
    defaultLevelCombo->setObjectName(QStringLiteral("defaultLevelCombo"));
    defaultLevelCombo->addItem(QString(), QStringLiteral("B_B"));
    defaultLevelCombo->addItem(QString(), QStringLiteral("B_T"));
    defaultLevelCombo->addItem(QString(), QStringLiteral("B_LT"));
    defaultLevelCombo->addItem(QString(), QStringLiteral("B_LTA"));
    defaultLevelLabel = new QLabel(signingTab);
    signingForm->addRow(defaultLevelLabel, defaultLevelCombo);

    auto* outputRow = new QHBoxLayout;
    defaultOutputFolder = new QLineEdit(signingTab);
    defaultOutputFolder->setObjectName(QStringLiteral("defaultOutputFolder"));
    defaultOutputFolder->setReadOnly(true);
    browseOutputBtn = new QPushButton(signingTab);
    outputRow->addWidget(defaultOutputFolder, 1);
    outputRow->addWidget(browseOutputBtn);
    defaultOutputLabel = new QLabel(signingTab);
    signingForm->addRow(defaultOutputLabel, outputRow);

    // The reason and the location a signature carries by default. They used to
    // be whatever the wizard was last told; they are the agent's preference now,
    // and the wizard prefills from them.
    defaultReasonEdit = new QLineEdit(signingTab);
    defaultReasonEdit->setObjectName(QStringLiteral("defaultReasonEdit"));
    defaultReasonLabel = new QLabel(signingTab);
    signingForm->addRow(defaultReasonLabel, defaultReasonEdit);

    defaultLocationEdit = new QLineEdit(signingTab);
    defaultLocationEdit->setObjectName(QStringLiteral("defaultLocationEdit"));
    defaultLocationLabel = new QLabel(signingTab);
    signingForm->addRow(defaultLocationLabel, defaultLocationEdit);

    // Informational only: the agent records which timestamp authority it last
    // used. The key is read-only on the wire, so there is nothing to edit.
    lastTsaValue = new QLabel(signingTab);
    lastTsaValue->setObjectName(QStringLiteral("lastTsaValue"));
    lastTsaValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    lastTsaLabel = new QLabel(signingTab);
    signingForm->addRow(lastTsaLabel, lastTsaValue);

    signingLayout->addLayout(signingForm);

    // TSA servers
    tsaServersLabel = new QLabel(signingTab);
    signingLayout->addWidget(tsaServersLabel);
    tsaList = new QListWidget(signingTab);
    tsaList->setObjectName(QStringLiteral("tsaList"));
    // Sized to its rows, by the same rule the trust tab's box follows (see
    // sizeListToContents). Two list boxes on two tabs of one dialog, one
    // ballooning to fill its tab and the other hugging its entries, would read
    // as two different kinds of control.
    tsaList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    signingLayout->addWidget(tsaList);
    // populateTsaList() seeds the list and adds the translated "Add"
    // sentinel row; called from retranslateUi() at end of ctor.

    auto* tsaDelegate = new TsaItemDelegate(tsaList, this);
    tsaList->setItemDelegate(tsaDelegate);
    connect(tsaDelegate, &TsaItemDelegate::addRequested, this, &SettingsDialog::onTsaAddRequested);
    connect(tsaDelegate, &TsaItemDelegate::removeRequested, this, [this](int row) {
        if (row >= 0 && row < tsaList->count() &&
            tsaList->item(row)->data(Qt::UserRole).toString() == QStringLiteral("custom")) {
            delete tsaList->takeItem(row);
            sizeListToContents(tsaList);
        }
    });

    // The tab-wide restore. It hands back EVERY key this tab owns, not the
    // list it happens to sit under, so a rule separates it from the last
    // setting above: without one it reads as a third control of the TSA box.
    // The trust tab draws the same rule in the same place, so the two tabs
    // answer the question "what does this button reach" the same way.
    signingLayout->addWidget(makeSeparator(signingTab));
    signingRestoreDefaultsBtn = new QPushButton(signingTab);
    signingRestoreDefaultsBtn->setObjectName(QStringLiteral("signingRestoreDefaultsButton"));
    auto* signingButtonRow = new QHBoxLayout;
    signingButtonRow->addStretch();
    signingButtonRow->addWidget(signingRestoreDefaultsBtn);
    signingLayout->addLayout(signingButtonRow);

    connect(signingRestoreDefaultsBtn, &QPushButton::clicked, this, [this]() { restoreDefaults(signingKeys()); });

    signingLayout->addStretch();

    tabs->addTab(signingTab, QString());

    connect(browseOutputBtn, &QPushButton::clicked, this, [this]() {
        const QString title =
            qtTrId("lc-sign-select-output-folder"); // i18n-audit: ignore D2, transient file dialog — qtTrId evaluated
                                                    // at click time, dialog discarded after exec()
        QString dir = QFileDialog::getExistingDirectory(this, title, defaultOutputFolder->text());
        if (!dir.isEmpty())
            defaultOutputFolder->setText(dir);
    });

    // --- Trust tab ---
    //
    // Two settings, each inside its own frame with its own title: the trusted
    // lists a signature is validated against, and the country-signing anchors a
    // travel document is checked against. The anchor half used to be loose text
    // under the list, which read as a footnote to the list rather than as the
    // second setting the tab carries.
    auto* trustTab = new QWidget(this);
    auto* trustLayout = new QVBoxLayout(trustTab);

    tlGroup = new QGroupBox(trustTab);
    tlGroup->setObjectName(QStringLiteral("tlGroup"));
    auto* tlGroupLayout = new QVBoxLayout(tlGroup);
    tlList = new QListWidget(tlGroup);
    tlList->setObjectName(QStringLiteral("tlList"));
    // Sized to the rows it holds (see sizeListToContents), not to whatever
    // vertical space is going spare. A list widget defaults to Expanding, and
    // with one entry in it that turned this frame into two hundred pixels of
    // white with a line of text at the top.
    tlList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tlGroupLayout->addWidget(tlList);
    trustLayout->addWidget(tlGroup);
    // populateTlList() seeds the list and adds the translated "Add"
    // sentinel row; called from retranslateUi() at end of ctor.

    auto* tlDelegate = new TlItemDelegate(tlList, this);
    tlList->setItemDelegate(tlDelegate);
    connect(tlDelegate, &TlItemDelegate::addRequested, this, &SettingsDialog::onTlAddRequested);
    connect(tlDelegate, &TlItemDelegate::removeRequested, this, [this](int row) {
        if (row >= 0 && row < tlList->count() &&
            tlList->item(row)->data(TlItemDelegate::TypeRole).toString() == QStringLiteral("custom")) {
            delete tlList->takeItem(row);
            sizeListToContents(tlList);
        }
    });

    // Country-signing anchors. Deliberately NOT a second source list beside
    // the trusted ones: nothing fetches from such a list, and an "add source"
    // that silently does nothing leaves a reader believing they configured
    // something. What there is instead is the one step a person can actually
    // take — hand the agent a signed master list — and an honest account of
    // what came back.
    cscaGroup = new QGroupBox(trustTab);
    cscaGroup->setObjectName(QStringLiteral("cscaGroup"));
    auto* cscaGroupLayout = new QVBoxLayout(cscaGroup);

    cscaSummaryLabel = new QLabel(cscaGroup);
    cscaSummaryLabel->setObjectName(QStringLiteral("cscaSummaryLabel"));
    cscaSummaryLabel->setWordWrap(true);
    cscaSummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cscaGroupLayout->addWidget(cscaSummaryLabel);

    cscaStatusLabel = new QLabel(cscaGroup);
    cscaStatusLabel->setObjectName(QStringLiteral("cscaStatusLabel"));
    cscaStatusLabel->setWordWrap(true);
    cscaStatusLabel->setVisible(false);
    cscaGroupLayout->addWidget(cscaStatusLabel);

    // Everything above this rule is a READING of what the agent holds;
    // everything below it is ADVICE about how to change that. They were two
    // paragraphs of identical text in one frame, and the count ran straight on
    // into the instructions as though it were the first sentence of them.
    cscaGroupLayout->addWidget(makeSeparator(cscaGroup));

    // What a master list is, where the public one is downloaded from, and why
    // this application will not go and get it. Without all three the import
    // button is a control nobody can reach: the feature needs a file that only
    // exists somewhere else, and the name of the thing says nothing about where
    // that is.
    //
    // Three sentences, three kinds of thing, so three labels rather than one
    // block. The middle one — WHERE — is what a reader opened this frame to
    // find, so it keeps the body voice and carries the address as a live link,
    // to be clicked or copied rather than transcribed by eye. The definition
    // above and the reason below are context a second reader already has, and
    // they are set quiet so that reader's eye can pass over them.
    cscaWhatLabel = new QLabel(cscaGroup);
    cscaWhatLabel->setObjectName(QStringLiteral("cscaWhatLabel"));
    cscaWhatLabel->setWordWrap(true);
    makeSecondary(cscaWhatLabel);
    cscaGroupLayout->addWidget(cscaWhatLabel);

    cscaHelpLabel = new QLabel(cscaGroup);
    cscaHelpLabel->setObjectName(QStringLiteral("cscaHelpLabel"));
    cscaHelpLabel->setWordWrap(true);
    cscaHelpLabel->setTextFormat(Qt::RichText);
    cscaHelpLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    cscaHelpLabel->setOpenExternalLinks(true);
    cscaGroupLayout->addWidget(cscaHelpLabel);

    cscaManualLabel = new QLabel(cscaGroup);
    cscaManualLabel->setObjectName(QStringLiteral("cscaManualLabel"));
    cscaManualLabel->setWordWrap(true);
    makeSecondary(cscaManualLabel);
    cscaGroupLayout->addWidget(cscaManualLabel);

    // The import belongs to the anchors, so it sits inside their frame, under
    // the sentence that tells a reader what to import. The tab-wide restore
    // stays outside it, in the same place the signing tab keeps its own.
    cscaImportButton = new QPushButton(cscaGroup);
    cscaImportButton->setObjectName(QStringLiteral("cscaImportButton"));
    auto* cscaButtonRow = new QHBoxLayout;
    cscaButtonRow->addStretch();
    cscaButtonRow->addWidget(cscaImportButton);
    cscaGroupLayout->addLayout(cscaButtonRow);

    trustLayout->addWidget(cscaGroup);

    connect(cscaImportButton, &QPushButton::clicked, this, &SettingsDialog::onCscaImportRequested);

    // Outside both frames, under the same rule the signing tab draws: this one
    // hands back every key the TAB owns, and standing loose under two titled
    // boxes it read as belonging to neither.
    trustLayout->addWidget(makeSeparator(trustTab));
    trustRestoreDefaultsBtn = new QPushButton(trustTab);
    trustRestoreDefaultsBtn->setObjectName(QStringLiteral("trustRestoreDefaultsButton"));
    auto* trustButtonRow = new QHBoxLayout;
    trustButtonRow->addStretch();
    trustButtonRow->addWidget(trustRestoreDefaultsBtn);
    trustLayout->addLayout(trustButtonRow);

    connect(trustRestoreDefaultsBtn, &QPushButton::clicked, this, [this]() { restoreDefaults(trustKeys()); });

    trustLayout->addStretch();

    tabs->addTab(trustTab, QString());

    // Why the operation-backed tabs are dark, said once, under the tabs.
    needsAgentLabel = new QLabel(this);
    needsAgentLabel->setObjectName(QStringLiteral("needsAgentLabel"));
    needsAgentLabel->setWordWrap(true);
    layout->addWidget(needsAgentLabel);

    // The refusal line. It is the whole answer to a rejected write: rendered
    // once, never followed by a retry.
    statusLabel = new QLabel(this);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusLabel->setWordWrap(true);
    statusLabel->setVisible(false);
    layout->addWidget(statusLabel);

    // --- Button box ---
    auto* buttonBox = new librecelik::ButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto* okButton = buttonBox->button(QDialogButtonBox::Ok))
        okButton->setObjectName(QStringLiteral("okButton"));
    if (auto* cancelButton = buttonBox->button(QDialogButtonBox::Cancel))
        cancelButton->setObjectName(QStringLiteral("cancelButton"));
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        // A refused write leaves the dialog open with the reason on screen:
        // closing over it would hide the only account the human gets of why
        // the change did not take.
        if (saveSettings())
            accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, [this]() {
        // Restore the original language if the user changed it during live preview
        QString currentLocale = languageCombo->currentData().toString();
        if (currentLocale != originalLocale)
            emit languageChanged(originalLocale);
        reject();
    });

    if (gateway != nullptr) {
        connect(gateway, &librecelik::agent::AgentGateway::presenceChanged, this,
                [this](PresenceState) { applyPresence(); });
        // The agent's configuration is shared. Another client's write lands
        // here as a fresh snapshot rather than as a stale dialog.
        connect(gateway, &librecelik::agent::AgentGateway::configChanged, this, [this](const QString&) {
            if (!writeInFlight)
                loadConfig();
        });
    }

    // The snapshot has to be in hand before the first render: retranslateUi()
    // rebuilds both lists, and they are a rendering of exactly this map.
    config = gateway != nullptr ? gateway->configSnapshot() : QVariantMap();

    // Display-only prefill for the trust-tier lists, read once from the legacy
    // store. These keys are polkit `auth_self` on every write, so the import
    // never sends them by itself (see agent/settingsimport.h): the human sees
    // the old values here, and they reach the agent only if a Save is clicked —
    // which is exactly where the authorisation ceremony belongs.
    {
        QSettings legacy(settings::kOrganization, settings::kApplication);
        for (const auto& item : librecelik::agent::buildConfig1Import(legacy).trustTier)
            trustPrefill.insert(item.wireKey, item.value);
    }

    // Apply translations and seed the dynamic lists. This is the single
    // source of truth for translatable widget state; LanguageChange
    // re-runs retranslateUi(), which re-runs populateTsaList /
    // populateTlList so the "Add" sentinel rows pick up the new locale.
    retranslateUi();
    loadSettings();
    loadConfig();
    applyPresence();
}

void SettingsDialog::importMasterList(int masterListFd)
{
    if (gateway == nullptr || gateway->presence() != PresenceState::Ready)
        return;

    const auto imported = gateway->importCscaMasterList(masterListFd);
    if (imported.has_value()) {
        cscaState = anchorStateAsMap(*imported);
        cscaOutcome = CscaImportOutcome::Installed;
    } else {
        // A refusal installed nothing and gave up nothing already held, so
        // whatever the summary said about the anchors is still exactly as
        // true (or as unknown) as it was — it is left alone on purpose.
        switch (imported.error()) {
        case LibreSCRS::AgentClient::SyncError::MasterListReplayed:
            // "Strictly newer" admits no equality: the same file again lands
            // here, and so does an older one. Both are answers a person can
            // act on, which is why the agent names this refusal separately.
            cscaOutcome = CscaImportOutcome::Replayed;
            break;
        case LibreSCRS::AgentClient::SyncError::NotAuthorized:
            cscaOutcome = CscaImportOutcome::Unauthorized;
            break;
        default:
            // Every "this file is not a usable master list" refusal the agent
            // names is outside the closed error vocabulary and arrives here.
            cscaOutcome = CscaImportOutcome::Refused;
            break;
        }
    }
    renderCscaState();
}

void SettingsDialog::importMasterListFile(const QString& path)
{
    if (path.isEmpty() || gateway == nullptr || gateway->presence() != PresenceState::Ready)
        return;

    // O_CLOEXEC: a descriptor handed to the agent must never survive into a
    // child process. The handle closes it on the way out whatever the answer
    // was — the call BORROWS it and duplicates what it puts on the wire.
    LibreSCRS::AgentClient::FdHandle file{::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC)};
    if (!file.valid()) {
        cscaOutcome = CscaImportOutcome::Unreadable;
        renderCscaState();
        return;
    }
    importMasterList(file.get());
}

void SettingsDialog::onCscaImportRequested()
{
    const QString title =
        qtTrId("lc-settings-csca-import-title"); // i18n-audit: ignore D2, transient file dialog — qtTrId evaluated
                                                 // at click time, dialog discarded after exec()
    const QString filter = qtTrId("lc-settings-csca-import-filter");
    const QString path = QFileDialog::getOpenFileName(this, title, QString(), filter);
    if (path.isEmpty())
        return; // cancelled: nothing was chosen, so there is nothing to report
    importMasterListFile(path);
}

void SettingsDialog::renderCscaState()
{
    if (cscaState.isEmpty()) {
        // Nothing has been imported — or the agent threw away a record that no
        // longer matched its anchor cache. Indistinguishable from here, and
        // both are honestly nothing installed.
        cscaSummaryLabel->setText(qtTrId("lc-settings-csca-state-none"));
    } else {
        QStringList lines;
        // A count is printed only where the agent SENT one. An absent member
        // is absent, never zero, and "Installed anchors: 0" over a state that
        // named no count is a reading nobody took.
        //
        // ANCHORS, never "roots": the count includes CSCA link certificates,
        // which are anchors like any other and are not self-signed roots.
        if (cscaState.contains(kCscaAnchors)) {
            lines << qtTrId("lc-settings-csca-state-anchors")
                         .arg(cscaState.value(kCscaAnchors).toUInt())
                         .arg(cscaState.value(kCscaIssuers).toUInt());
        }
        if (cscaState.contains(kCscaSignedAt)) {
            // Only when the accepted list carried one. CMS permits an absent
            // signing time, the agent sends no epoch sentinel for it, and a
            // zero read back as a date would put 1970 on screen as a real
            // stamp. Written as an ISO date in UTC: the stamp belongs to the
            // publisher, and a local-time rendering can move it by a day.
            const QDateTime signedAt =
                QDateTime::fromSecsSinceEpoch(cscaState.value(kCscaSignedAt).toLongLong(), QTimeZone::UTC);
            lines << qtTrId("lc-settings-csca-state-signed").arg(signedAt.date().toString(Qt::ISODate));
        }
        // The FALSE is the value worth surfacing: staying silent leaves a
        // reader unable to tell "a later list that is not newer will be
        // refused" from "that cannot be checked at all". An absent member
        // reads as false here, which is the only direction a missing key may
        // move this sentence — it can withhold a guarantee, never invent one.
        lines << (cscaState.value(kCscaReplayRefusalActive).toBool() ? qtTrId("lc-settings-csca-rollback-on")
                                                                     : qtTrId("lc-settings-csca-rollback-off"));
        cscaSummaryLabel->setText(lines.join(QLatin1Char('\n')));
    }

    QString status;
    switch (cscaOutcome) {
    case CscaImportOutcome::None:
        break;
    case CscaImportOutcome::Installed:
        status = qtTrId("lc-settings-csca-installed");
        break;
    case CscaImportOutcome::Replayed:
        status = qtTrId("lc-settings-csca-replayed");
        break;
    case CscaImportOutcome::Unauthorized:
        status = qtTrId("lc-settings-config-unauthorized");
        break;
    case CscaImportOutcome::Refused:
        status = qtTrId("lc-settings-csca-refused");
        break;
    case CscaImportOutcome::Unreadable:
        status = qtTrId("lc-settings-csca-unreadable");
        break;
    }
    cscaStatusLabel->setText(status);
    cscaStatusLabel->setVisible(!status.isEmpty());
}

void SettingsDialog::loadSettings()
{
    QSettings settings(settings::kOrganization, settings::kApplication);

    // Resolve "what language is the UI actually rendering in?" — the same
    // chain LibreCelik's ctor uses. If kLanguage is empty (fresh install,
    // never explicitly chosen), this returns the locale that the system
    // fallback landed on, so the dropdown shows the language the user
    // actually sees rather than a stale-but-unset preference.
    QStringList supported;
    for (int i = 0; i < languageCombo->count(); ++i) {
        supported << languageCombo->itemData(i).toString();
    }
    const QString locale = utils::resolveActiveLocale(settings.value(settings::kLanguage, QString()).toString(),
                                                      supported, QLocale::system().uiLanguages());
    originalLocale = locale;
    int langIdx = languageCombo->findData(locale);
    if (langIdx >= 0)
        languageCombo->setCurrentIndex(langIdx);

    defaultOutputFolder->setText(settings.value(settings::kSigningDefaultOutputFolder).toString());
}

void SettingsDialog::loadConfig()
{
    config = gateway != nullptr ? gateway->configSnapshot() : QVariantMap();

    const int levelIdx = defaultLevelCombo->findData(uiLevelToken(config.value(kDefaultLevel).toString()));
    defaultLevelCombo->setCurrentIndex(levelIdx >= 0 ? levelIdx : 0);
    defaultReasonEdit->setText(config.value(kDefaultReason).toString());
    defaultLocationEdit->setText(config.value(kDefaultLocation).toString());
    lastTsaValue->setText(config.value(kLastTsaUrl).toString());

    // What the agent HOLDS in country-signing anchors, read like any other
    // setting. It is served read-only and without an import having happened,
    // which is what lets the Trust tab account for the anchors on open instead
    // of saying it cannot know.
    cscaState = config.value(kCscaAnchorState).toMap();

    populateTsaList();
    populateTlList();
    renderCscaState();
}

void SettingsDialog::applyPresence()
{
    const bool ready = gateway != nullptr && gateway->presence() == PresenceState::Ready;
    tabs->setTabEnabled(kSigningTabIndex, ready);
    tabs->setTabEnabled(kTrustTabIndex, ready);
    needsAgentLabel->setVisible(!ready);
}

void SettingsDialog::renderStatus()
{
    if (!lastRefusal.has_value()) {
        statusLabel->clear();
        statusLabel->setVisible(false);
        return;
    }
    statusLabel->setText(*lastRefusal == LibreSCRS::AgentClient::SyncError::NotAuthorized
                             ? qtTrId("lc-settings-config-unauthorized")
                             : qtTrId("lc-settings-config-refused"));
    statusLabel->setVisible(true);
}

bool SettingsDialog::saveSettings()
{
    QSettings settings(settings::kOrganization, settings::kApplication);

    // Language (persisted on OK, live preview was immediate)
    settings.setValue(settings::kLanguage, languageCombo->currentData().toString());
    settings.setValue(settings::kSigningDefaultOutputFolder, defaultOutputFolder->text());

    lastRefusal.reset();
    renderStatus();

    if (gateway == nullptr || gateway->presence() != PresenceState::Ready)
        return true; // the operation-backed tabs are dark; there is nothing to write

    // Collect FIRST, write SECOND: every accepted write announces itself
    // through configChanged, and a snapshot re-read mid-run would drop the
    // edits the remaining keys still have to carry.
    //
    // Each comparison is against the snapshot read in the SAME type the
    // control produces, so a key the agent does not carry compares as that
    // type's empty value rather than as a mismatching invalid variant — an
    // untouched control must not manufacture a write. The level round-trips
    // through the token map for exactly that reason: the combo cannot
    // represent "absent", and an absent level is the baseline one.
    QList<QPair<QString, QVariant>> pending;
    const QString level = wireLevelToken(defaultLevelCombo->currentData().toString());
    if (level != wireLevelToken(uiLevelToken(config.value(kDefaultLevel).toString())))
        pending.append(qMakePair(QString(kDefaultLevel), QVariant(level)));
    if (defaultReasonEdit->text() != config.value(kDefaultReason).toString())
        pending.append(qMakePair(QString(kDefaultReason), QVariant(defaultReasonEdit->text())));
    if (defaultLocationEdit->text() != config.value(kDefaultLocation).toString())
        pending.append(qMakePair(QString(kDefaultLocation), QVariant(defaultLocationEdit->text())));
    if (const QStringList urls = tsaUrlsFromList(); urls != config.value(kTsaUrls).toStringList())
        pending.append(qMakePair(QString(kTsaUrls), QVariant(urls)));
    if (const QVariantList sources = tslSourcesFromList(); sources != config.value(kTslSources).toList())
        pending.append(qMakePair(QString(kTslSources), QVariant(sources)));

    writeInFlight = true;
    bool accepted = true;
    for (const auto& entry : std::as_const(pending)) {
        const auto refusal = gateway->setConfigValue(entry.first, entry.second);
        if (refusal.has_value()) {
            // One refusal ends the Save. Carrying on would mean a SECOND
            // authorisation ceremony for the next trust-tier key in the same
            // click, and the human has already answered this one.
            lastRefusal = refusal;
            accepted = false;
            break;
        }
    }
    writeInFlight = false;

    // Only a clean run re-reads: after a refusal the edits stay on screen, so
    // "Save again" still has something to save.
    if (accepted)
        loadConfig();
    renderStatus();
    return accepted;
}

void SettingsDialog::restoreDefaults(const QStringList& keys)
{
    if (gateway == nullptr)
        return;

    lastRefusal.reset();
    writeInFlight = true;
    for (const QString& key : keys) {
        const auto refusal = gateway->resetConfigValue(key);
        if (refusal.has_value()) {
            lastRefusal = refusal;
            break;
        }
    }
    writeInFlight = false;

    // The agent's defaults are whatever it answers now — LC never re-invents
    // them, so the tab is redrawn from a fresh snapshot rather than from a
    // local table of what the defaults "should" be.
    loadConfig();
    renderStatus();
}

QStringList SettingsDialog::tsaUrlsFromList() const
{
    QStringList urls;
    for (int row = 0; row < tsaList->count(); ++row) {
        if (tsaList->item(row)->data(Qt::UserRole).toString() == QStringLiteral("add"))
            continue;
        urls.append(tsaList->item(row)->text());
    }
    return urls;
}

QVariantList SettingsDialog::tslSourcesFromList() const
{
    QVariantList sources;
    for (int row = 0; row < tlList->count(); ++row) {
        auto* item = tlList->item(row);
        if (item->data(TlItemDelegate::TypeRole).toString() == QStringLiteral("add"))
            continue;
        sources.append(QVariant(QVariantList{item->text(), item->data(TlItemDelegate::IsLotlRole).toBool(),
                                             item->data(TlItemDelegate::EagerRole).toBool()}));
    }
    return sources;
}

void SettingsDialog::populateTsaList()
{
    tsaList->clear();

    // The agent's own value wins; the legacy prefill is shown only while the
    // agent still carries nothing under this key.
    QStringList rawUrls = config.value(kTsaUrls).toStringList();
    if (rawUrls.isEmpty())
        rawUrls = trustPrefill.value(kTsaUrls).toStringList();

    for (const QString& rawUrl : std::as_const(rawUrls)) {
        const QString url = rawUrl.trimmed();
        // Re-validate on read: the agent's configuration is a shared store, and
        // a value another writer put there is not this dialog's own add-time
        // check having passed.
        if (url.isEmpty() || !isValidServiceUrl(url))
            continue;
        auto* item = new QListWidgetItem(url, tsaList);
        item->setData(Qt::UserRole, QStringLiteral("custom"));
    }

    // Add row
    auto* addItem = new QListWidgetItem(qtTrId("lc-sign-tsa-add-item"), tsaList);
    addItem->setData(Qt::UserRole, QStringLiteral("add"));

    sizeListToContents(tsaList);
}

void SettingsDialog::onTsaAddRequested()
{
    const auto title = qtTrId("lc-sign-tsa-add-title"); // i18n-audit: ignore D2, transient input dialog — opened on
                                                        // user click, qtTrId evaluated at call time
    const auto prompt = qtTrId("lc-sign-tsa-add-prompt");
    QString url = librecelik::dialogs::getText(this, title, prompt);
    url = url.trimmed();
    if (url.isEmpty())
        return;
    if (!url.startsWith(QStringLiteral("https://")) && !url.startsWith(QStringLiteral("http://")))
        url.prepend(QStringLiteral("https://"));
    if (!isValidServiceUrl(url)) {
        librecelik::dialogs::warning(this, qtTrId("lc-settings-invalid-url-title"),
                                     qtTrId("lc-settings-invalid-url-msg"));
        return;
    }
    for (int i = 0; i < tsaList->count(); ++i) {
        if (tsaList->item(i)->text() == url)
            return;
    }
    int addRow = tsaList->count() - 1;
    auto* item = new QListWidgetItem(url);
    item->setData(Qt::UserRole, QStringLiteral("custom"));
    tsaList->insertItem(addRow, item);
    sizeListToContents(tsaList);
}

void SettingsDialog::populateTlList()
{
    tlList->clear();

    // Same prefill rule as the TSA list: display-only, agent value first.
    QVariantList entries = config.value(kTslSources).toList();
    if (entries.isEmpty())
        entries = trustPrefill.value(kTslSources).toList();

    for (const QVariant& entry : std::as_const(entries)) {
        // [url, lotl, eager] — the shape both transports normalise to.
        const QVariantList row = entry.toList();
        if (row.size() != 3)
            continue;
        const QString url = row.at(0).toString().trimmed();
        // Re-validate on read (see populateTsaList for rationale).
        if (url.isEmpty() || !isValidServiceUrl(url))
            continue;
        auto* item = new QListWidgetItem(url, tlList);
        item->setData(TlItemDelegate::TypeRole, QStringLiteral("custom"));
        item->setData(TlItemDelegate::IsLotlRole, row.at(1).toBool());
        item->setData(TlItemDelegate::EagerRole, row.at(2).toBool());
    }

    auto* addItem = new QListWidgetItem(qtTrId("lc-settings-tl-add-item"), tlList);
    addItem->setData(TlItemDelegate::TypeRole, QStringLiteral("add"));

    sizeListToContents(tlList);
}

void SettingsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void SettingsDialog::retranslateUi()
{
    setWindowTitle(qtTrId("lc-settings-title"));
    tabs->setTabText(0, qtTrId("lc-settings-tab-general"));
    tabs->setTabText(1, qtTrId("lc-settings-tab-signing"));
    tabs->setTabText(2, qtTrId("lc-settings-tab-trust"));

    languageLabel->setText(qtTrId("lc-settings-language"));
    defaultLevelLabel->setText(qtTrId("lc-settings-default-level"));
    defaultOutputLabel->setText(qtTrId("lc-settings-default-output"));
    defaultOutputFolder->setPlaceholderText(qtTrId("lc-settings-output-placeholder"));
    browseOutputBtn->setText(qtTrId("lc-sign-change-folder"));
    defaultReasonLabel->setText(qtTrId("lc-sign-visual-reason"));
    defaultReasonEdit->setPlaceholderText(qtTrId("lc-sign-visual-reason-placeholder"));
    defaultLocationLabel->setText(qtTrId("lc-sign-visual-location"));
    defaultLocationEdit->setPlaceholderText(qtTrId("lc-sign-visual-location-placeholder"));
    lastTsaLabel->setText(qtTrId("lc-settings-last-tsa"));
    tsaServersLabel->setText(qtTrId("lc-settings-tsa-servers"));
    tlGroup->setTitle(qtTrId("lc-settings-tl-servers"));
    cscaGroup->setTitle(qtTrId("lc-settings-csca-anchors"));
    // The one address named on this screen, and the only one this project has
    // checked. Locale-stable, so it is composed here rather than carried inside
    // a translatable sentence where three catalogues could drift apart on it.
    const QString icaoPkdLink = QStringLiteral("<a href=\"%1\">%1</a>").arg(kIcaoPkdUrl);
    cscaWhatLabel->setText(qtTrId("lc-settings-csca-what"));
    cscaHelpLabel->setText(qtTrId("lc-settings-csca-where").arg(icaoPkdLink));
    cscaManualLabel->setText(qtTrId("lc-settings-csca-manual"));
    cscaImportButton->setText(qtTrId("lc-settings-csca-import"));
    signingRestoreDefaultsBtn->setText(qtTrId("lc-btn-restore-defaults"));
    trustRestoreDefaultsBtn->setText(qtTrId("lc-btn-restore-defaults"));
    needsAgentLabel->setText(qtTrId("lc-settings-needs-agent"));

    int levelIdx = defaultLevelCombo->currentIndex();
    defaultLevelCombo->setItemText(0, qtTrId("lc-sign-level-bb"));
    defaultLevelCombo->setItemText(1, qtTrId("lc-sign-level-bt"));
    defaultLevelCombo->setItemText(2, qtTrId("lc-sign-level-blt"));
    defaultLevelCombo->setItemText(3, qtTrId("lc-sign-level-blta"));
    defaultLevelCombo->setCurrentIndex(levelIdx);

    populateTsaList();
    populateTlList();
    renderCscaState();
    renderStatus();
}

void SettingsDialog::onTlAddRequested()
{
    QDialog dlg(this);
    dlg.setWindowTitle(qtTrId("lc-settings-tl-add-title")); // i18n-audit: ignore D2, transient inline dialog — qtTrId
                                                            // evaluated at click time, dlg destructed after exec()
    dlg.setMinimumWidth(400);
    auto* layout = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout;

    auto* urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText(QStringLiteral("https://..."));
    form->addRow(QStringLiteral("URL:"), urlEdit);

    auto* typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(QStringLiteral("TL"), false);
    typeCombo->addItem(QStringLiteral("LOTL"), true);
    form->addRow(qtTrId("lc-settings-tl-type"), typeCombo);

    auto* loadCombo = new QComboBox(&dlg);
    loadCombo->addItem(QStringLiteral("Eager"), true);
    loadCombo->addItem(QStringLiteral("Lazy"), false);
    form->addRow(qtTrId("lc-settings-tl-loading"), loadCombo);

    layout->addLayout(form);
    auto* btnBox = new librecelik::ButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btnBox);
    connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    QString url = urlEdit->text().trimmed();
    if (url.isEmpty())
        return;
    if (!url.startsWith(QStringLiteral("https://")) && !url.startsWith(QStringLiteral("http://")))
        url.prepend(QStringLiteral("https://"));
    if (!isValidServiceUrl(url)) {
        librecelik::dialogs::warning(this, qtTrId("lc-settings-invalid-url-title"),
                                     qtTrId("lc-settings-invalid-url-msg"));
        return;
    }

    int addRow = tlList->count() - 1;
    auto* item = new QListWidgetItem(url);
    item->setData(TlItemDelegate::TypeRole, QStringLiteral("custom"));
    item->setData(TlItemDelegate::IsLotlRole, typeCombo->currentData().toBool());
    item->setData(TlItemDelegate::EagerRole, loadCombo->currentData().toBool());
    tlList->insertItem(addRow, item);
    sizeListToContents(tlList);
}
