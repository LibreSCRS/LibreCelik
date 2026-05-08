// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificateviewerwidget.h"
#include "cert_format.h"
#include "certificatehierarchymodel.h"
#include "certificatepropertiesmodel.h"
#include "ui_certificateviewerwidget.h"

#include <LibreSCRS/Auth/ErrorKeys.h>
#include <LibreSCRS/Certificate/ParsedCertificate.h>

#include <QEvent>
#include <QLineEdit>

namespace lcc = LibreSCRS::Certificate;
namespace cf = librecelik::cert_format;

namespace {

QString fromStd(const std::string& s)
{
    return QString::fromStdString(s);
}

QString keyUsageString(const lcc::ParsedCertificate& cert)
{
    auto ku = cert.keyUsage();
    if (!ku)
        return {};
    return cf::keyUsageToString(*ku);
}

} // namespace

CertificateViewerWidget::CertificateViewerWidget(std::vector<std::uint8_t> der,
                                                 std::shared_ptr<const LibreSCRS::Trust::TrustStore> store,
                                                 QWidget* parent)
    : QWidget(parent), ui(new Ui::CertificateViewerWidget), leafCertDer(std::move(der)),
      // Field carries an "uninitialised" ParseError until fromDer() is invoked
      // below. ParsedCertificate has no public default ctor, so std::expected
      // cannot itself be default-constructed; we seed an explicit unexpected.
      parsedCert(std::unexpected{lcc::ParsedCertificate::ParseError{lcc::ParsedCertificate::ParseError::Kind::Invalid,
                                                                    LibreSCRS::Auth::ErrorKeys::derInvalid(),
                                                                    std::string{"not yet parsed"}}}),
      trustStore(std::move(store))
{
    ui->setupUi(this);

    if (leafCertDer.empty()) {
        populateUnparseableTab();
        return;
    }

    parsedCert = lcc::ParsedCertificate::fromDer(leafCertDer);
    if (!parsedCert) {
        populateUnparseableTab();
        // Still wire the details model so the user sees the parse-error row
        // followed by a forensic hex dump of the raw DER bytes.
        auto* propsModel = new CertificatePropertiesModel(nullptr, std::span<const std::uint8_t>(leafCertDer), this);
        ui->detailsTreeView->setModel(propsModel);
        ui->detailsTreeView->expandAll();
        ui->detailsTreeView->resizeColumnToContents(0);
        connect(ui->detailsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &CertificateViewerWidget::onDetailsSelectionChanged);
        return;
    }

    populateGeneralTab();

    auto* propsModel = new CertificatePropertiesModel(&*parsedCert, this);
    ui->detailsTreeView->setModel(propsModel);
    ui->detailsTreeView->expandAll();
    ui->detailsTreeView->resizeColumnToContents(0);

    connect(ui->detailsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &CertificateViewerWidget::onDetailsSelectionChanged);

    auto* hierarchyModel = new CertificateHierarchyModel(&*parsedCert, trustStore, this);
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

void CertificateViewerWidget::populateGeneralTab()
{
    const auto& cert = *parsedCert;

    auto setFieldText = [](QLineEdit* edit, const QString& text) {
        edit->setText(text);
        edit->setCursorPosition(0);
    };

    const auto subject = cert.subject();
    const auto issuer = cert.issuer();

    setFieldText(ui->issuedToCNEdit, fromStd(subject.commonName()));
    setFieldText(ui->issuedToOEdit, fromStd(subject.organization()));
    setFieldText(ui->issuedToOUEdit, fromStd(subject.organizationalUnit()));
    setFieldText(ui->issuedToSerialEdit, cf::bytesToHex(std::span<const std::uint8_t>(cert.serialNumber())));

    setFieldText(ui->issuedByCNEdit, fromStd(issuer.commonName()));
    setFieldText(ui->issuedByOEdit, fromStd(issuer.organization()));
    setFieldText(ui->issuedByOUEdit, fromStd(issuer.organizationalUnit()));

    setFieldText(ui->notBeforeEdit, cf::formatTime(cert.notBefore()));
    setFieldText(ui->notAfterEdit, cf::formatTime(cert.notAfter()));

    setFieldText(ui->keyUsageEdit, keyUsageString(cert));
}

void CertificateViewerWidget::populateUnparseableTab()
{
    auto setFieldText = [](QLineEdit* edit, const QString& text) {
        edit->setText(text);
        edit->setCursorPosition(0);
    };

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
    // for the leaf — chain validation is chain-wide, not per-cert). Leave the
    // field empty rather than fabricating a misleading "Valid" indicator.
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
    // hand-coded "parse error" placeholder for the unparseable case so
    // the user-visible text in the General tab tracks the new language.
    ui->retranslateUi(this);
    if (leafCertDer.empty() || !parsedCert)
        populateUnparseableTab();
    // The two tree models (CertificatePropertiesModel /
    // CertificateHierarchyModel) self-retranslate via Qt::DisplayRole on
    // next paint (see model classes — flagged D1 and inline-allowlisted).
}
