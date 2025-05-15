// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKAPISESSIONFACTORY_H
#define CELIKAPISESSIONFACTORY_H

#include <memory>
#include "utils/utils.h"

class CelikAPIWrapper;
class CelikAPISession;

class CelikAPISessionFactory
{
public:
    static CelikAPISessionFactory& instance();

    std::unique_ptr<CelikAPISession> createCelikAPISession(const std::string& cardReader);

private:
    CelikAPISessionFactory(std::string certStorePath);
    ~CelikAPISessionFactory();
    DISABLE_COPY_MOVE(CelikAPISessionFactory);

    std::shared_ptr<CelikAPIWrapper> celikApiWrapper = nullptr;
    static const int API_VERSION = 3;
};

#endif // CELIKAPISESSIONFACTORY_H
