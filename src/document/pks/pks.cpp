// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QSpacerItem>
#include <QVBoxLayout>
#include "pks.h"
#include "pksreader.h"
#include "document/eid/changepindlg.h"
#include "document/tokensection.h"
#include "config.h"

Pks::Pks(std::string reader, QWidget* parent) : Document(parent)
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);

    tokenSection = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, this);
    tokenSection->setTitle(qtTrId("lc-pks-title"));
    tokenSection->setHeaderHeight(56);
    tokenSection->setExpanded(true);

    vl->addWidget(tokenSection);
    vl->addItem(new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

    pksReader = std::make_unique<PKSReader>(reader);

    connect(pksReader.get(), &PKSReader::certificateDataRead, tokenSection, &TokenSection::setCertificates);
    connect(pksReader.get(), &PKSReader::pinTriesLeftRead, tokenSection, &TokenSection::setPINStatus);
    connect(tokenSection, &TokenSection::changePINRequested, this, &Pks::openChangePinDlg);

    tokenSection->setPINVisible(true);
    pksReader->requestCertificates();
    pksReader->requestPINTriesLeft();
}

Pks::~Pks()
{
    pksReader.reset();
}

void Pks::openChangePinDlg()
{
    auto dlg = std::make_unique<ChangePinDlg>(this);
    connect(dlg.get(), &ChangePinDlg::pinChangeRequested, pksReader.get(), &PKSReader::requestChangePIN);
    connect(pksReader.get(), &PKSReader::pinTriesLeftRead, dlg.get(), &ChangePinDlg::onPinTriesLeftRead);
    connect(pksReader.get(), &PKSReader::pinChangeSuccess, dlg.get(), &ChangePinDlg::onPinChangeSuccess);
    connect(pksReader.get(), &PKSReader::pinChangeFailed, dlg.get(), &ChangePinDlg::onPinChangeFailed);
    pksReader->requestPINTriesLeft();
    dlg->exec();
}
