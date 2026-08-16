// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificateviewerwidget.h"
#include "certfields.h"
#include "certformat.h"
#include "certificatehierarchymodel.h"
#include "certificatepropertiesmodel.h"
#include "ui_certificateviewerwidget.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QLineEdit>
#include <QVariantMap>

namespace cf = librecelik::certformat;
namespace fields = librecelik::certfields;

using LibreSCRS::AgentClient::CertificateInfo;

namespace {

constexpr QLatin1StringView kGroupSubject{"subject"};
constexpr QLatin1StringView kGroupIssuer{"issuer"};
constexpr QLatin1StringView kGroupCert{"cert"};

void setFieldText(QLineEdit* edit, const QString& text)
{
    edit->setText(text);
    edit->setCursorPosition(0);
}

/// The common name out of a DN group, falling back to the typed member the
/// client library always fills (which is itself the common name).
[[nodiscard]] QString commonName(const QVariantMap& groups, QLatin1StringView group, const QString& typedFallback)
{
    const QString cn = fields::valueOf(groups, group, QLatin1StringView("cn"));
    return cn.isEmpty() ? typedFallback : cn;
}

} // namespace

CertificateViewerWidget::CertificateViewerWidget(const CertificateInfo& cert, QWidget* parent)
    : QWidget(parent), ui(new Ui::CertificateViewerWidget), certificate(cert), unparseable(fields::isUnparseable(cert))
{
    ui->setupUi(this);

    if (unparseable) {
        populateUnparseableTab();
        // Still wire the details model so the user sees the parse-error row
        // the agent reported (followed by a forensic hex dump once the raw
        // bytes arrive, see setForensicDer).
        installDetailsModel();
        return;
    }

    populateGeneralTab();
    installDetailsModel();

    auto* hierarchyModel = new CertificateHierarchyModel(certificate, this);
    ui->certPathTreeView->setModel(hierarchyModel);
    ui->certPathTreeView->expandAll();
    ui->certPathTreeView->resizeColumnToContents(0);

    connect(ui->certPathTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CertificateViewerWidget::onCertPathSelectionChanged);

    // Select the leaf node (deepest child) and populate summary.
    QModelIndex idx = hierarchyModel->index(0, 0);
    while (hierarchyModel->rowCount(idx) > 0)
        idx = hierarchyModel->index(0, 0, idx);
    if (idx.isValid())
        ui->certPathTreeView->setCurrentIndex(idx);
}

CertificateViewerWidget::~CertificateViewerWidget()
{
    delete ui;
}

void CertificateViewerWidget::setForensicDer(const QByteArray& der)
{
    if (forensicDer == der)
        return;
    forensicDer = der;
    // Only the unparseable path renders these bytes; for every other
    // certificate the detail rows are the agent's and rebuilding them would
    // change nothing.
    if (unparseable)
        installDetailsModel();
}

void CertificateViewerWidget::installDetailsModel()
{
    auto* previous = ui->detailsTreeView->model();
    auto* propsModel = new CertificatePropertiesModel(certificate, QByteArrayView(forensicDer), this);
    ui->detailsTreeView->setModel(propsModel);
    delete previous;
    ui->detailsTreeView->expandAll();
    ui->detailsTreeView->resizeColumnToContents(0);
    ui->detailsValueEdit->clear();

    connect(ui->detailsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CertificateViewerWidget::onDetailsSelectionChanged);
}

void CertificateViewerWidget::populateGeneralTab()
{
    const QVariantMap groups = fields::groupsOf(certificate);

    setFieldText(ui->issuedToCNEdit, commonName(groups, kGroupSubject, certificate.subject));
    setFieldText(ui->issuedToOEdit, fields::valueOf(groups, kGroupSubject, QLatin1StringView("o")));
    setFieldText(ui->issuedToOUEdit, fields::valueOf(groups, kGroupSubject, QLatin1StringView("ou")));
    setFieldText(ui->issuedToSerialEdit, fields::valueOf(groups, kGroupCert, QLatin1StringView("serial")));

    setFieldText(ui->issuedByCNEdit, commonName(groups, kGroupIssuer, certificate.issuer));
    setFieldText(ui->issuedByOEdit, fields::valueOf(groups, kGroupIssuer, QLatin1StringView("o")));
    setFieldText(ui->issuedByOUEdit, fields::valueOf(groups, kGroupIssuer, QLatin1StringView("ou")));

    setFieldText(ui->notBeforeEdit, cf::formatTime(certificate.notBefore));
    setFieldText(ui->notAfterEdit, cf::formatTime(certificate.notAfter));

    setFieldText(ui->keyUsageEdit, cf::keyUsageToString(certificate.keyUsageBits));
}

void CertificateViewerWidget::populateUnparseableTab()
{
    const QString placeholder = qtTrId("lc-cert-parse-error");
    setFieldText(ui->issuedToCNEdit, placeholder);
    setFieldText(ui->issuedToOEdit, QString());
    setFieldText(ui->issuedToOUEdit, QString());
    setFieldText(ui->issuedToSerialEdit, QString());
    setFieldText(ui->issuedByCNEdit, placeholder);
    setFieldText(ui->issuedByOEdit, QString());
    setFieldText(ui->issuedByOUEdit, QString());
    setFieldText(ui->notBeforeEdit, QString());
    setFieldText(ui->notAfterEdit, QString());
    setFieldText(ui->keyUsageEdit, QString());
}

void CertificateViewerWidget::onDetailsSelectionChanged(const QItemSelection& selected,
                                                        const QItemSelection& /*deselected*/)
{
    if (selected.indexes().isEmpty()) {
        ui->detailsValueEdit->clear();
        return;
    }

    QModelIndex index = selected.indexes().first();
    QModelIndex valueIndex = index.sibling(index.row(), 1);
    QString value = valueIndex.data(Qt::DisplayRole).toString();
    ui->detailsValueEdit->setPlainText(value);
}

void CertificateViewerWidget::onCertPathSelectionChanged(const QItemSelection& selected,
                                                         const QItemSelection& /*deselected*/)
{
    if (selected.indexes().isEmpty()) {
        ui->certPathCNEdit->clear();
        ui->certPathStatusEdit->clear();
        return;
    }

    QModelIndex index = selected.indexes().first();
    QString cn = index.data(Qt::DisplayRole).toString();
    ui->certPathCNEdit->setText(cn);
    ui->certPathCNEdit->setCursorPosition(0);

    QModelIndex valueIndex = index.sibling(index.row(), 1);
    QString status = valueIndex.data(Qt::DisplayRole).toString();
    // Empty status means a non-leaf row (the model only writes a status string
    // for the leaf — the agent's verdict is chain-wide, not per-certificate).
    // Leave the field empty rather than fabricating a misleading indicator.
    ui->certPathStatusEdit->setText(status);
}

void CertificateViewerWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void CertificateViewerWidget::retranslateUi()
{
    // The .ui-driven labels / tab titles retranslate automatically via
    // Ui::CertificateViewerWidget::retranslateUi() (called by Qt's
    // changeEvent on the .ui-attached QObjects). We re-apply the
    // hand-coded field text so the user-visible values in the General tab
    // track the new language — the KeyUsage line and the parse-error
    // placeholder are both localized.
    ui->retranslateUi(this);
    if (unparseable)
        populateUnparseableTab();
    else
        populateGeneralTab();
    // The two tree models (CertificatePropertiesModel /
    // CertificateHierarchyModel) self-retranslate via Qt::DisplayRole on
    // next paint (see model classes — flagged D1 and inline-allowlisted).
}
