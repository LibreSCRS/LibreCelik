// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "utils/libreceliklog.h"
#include "celikapisessionfactory.h"
#include "celikapisession.h"
#include "celikapiwrapper.h"

CelikAPISessionFactory& CelikAPISessionFactory::instance()
{
    static CelikAPISessionFactory casf(CERT_STORE_PATH);
    return casf;
}

CelikAPISessionFactory::CelikAPISessionFactory(std::string certStorePath)
{
    celikApiWrapper = std::make_shared<CelikAPIWrapper>();

    auto ret = celikApiWrapper->startup(API_VERSION);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "Cannot Startup CelikAPI";
        throw std::runtime_error("Cannot Startup CelikAPI");
    }

    ret = celikApiWrapper->setOption(EID_O_CERTIFICATE_FOLDER, (uintptr_t)(certStorePath.c_str()));
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "SetOption error: " << RVToString(ret);
        throw std::runtime_error("SetOption error");
    }
    qCDebug(librecSCRSCard) << "CelikAPISessionFactory created";
}

CelikAPISessionFactory::~CelikAPISessionFactory()
{
    auto ret = celikApiWrapper->cleanup();
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "Cannot cleanup";
    }
    qCDebug(librecSCRSCard) << "CelikAPISessionFactory destroyed";
}

std::unique_ptr<CelikAPISession> CelikAPISessionFactory::createCelikAPISession(const std::string& cardReader)
{
    return std::unique_ptr<CelikAPISession> (new CelikAPISession(cardReader, celikApiWrapper));
}
