# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
#
# Hybrid LibreMiddleware consumption: prefer find_package(CONFIG) when
# LIBRECELIK_USE_INSTALLED_LM=ON, otherwise fall back to the existing
# FetchContent build-tree path (the caller still drives FetchContent
# inline immediately after including this module).

option(LIBRECELIK_USE_INSTALLED_LM
       "Consume LibreMiddleware via find_package(CONFIG) instead of FetchContent" OFF)

if(LIBRECELIK_USE_INSTALLED_LM)
    find_package(LibreMiddleware 4.1 REQUIRED CONFIG)
    message(STATUS "LibreMiddleware: using installed package (CONFIG)")

    # Expose namespaced LibreSCRS::* aliases so the rest of LC sees the same
    # target names regardless of which consumption mode was selected. The
    # alias list mirrors the public surface exported by LM's
    # LibreMiddlewareTargets (Auth, Trust, Certificate, SmartCard, Plugin,
    # Signing, Secure).
    if(NOT TARGET LibreSCRS::SmartCard)
        add_library(LibreSCRS::SmartCard   ALIAS LibreMiddleware::SmartCard)
        add_library(LibreSCRS::Plugin      ALIAS LibreMiddleware::Plugin)
        add_library(LibreSCRS::Signing     ALIAS LibreMiddleware::Signing)
        add_library(LibreSCRS::Auth        ALIAS LibreMiddleware::Auth)
        add_library(LibreSCRS::Trust       ALIAS LibreMiddleware::Trust)
        add_library(LibreSCRS::Certificate ALIAS LibreMiddleware::Certificate)
        add_library(LibreSCRS::Secure      ALIAS LibreMiddleware::Secure)
    endif()
else()
    message(STATUS "LibreMiddleware: building from source (FetchContent)")
    # Caller continues with FetchContent_Declare(LibreMiddleware ...) inline.
endif()
