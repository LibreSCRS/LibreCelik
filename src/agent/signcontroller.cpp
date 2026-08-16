// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Out-of-line ctor/dtor: the key function that anchors SignController's vtable
// and typeinfo to this translation unit instead of emitting a copy in every
// TU that includes the header.

#include "agent/signcontroller.h"

namespace librecelik::agent {

SignController::SignController(QObject* parent) : QObject(parent) {}

SignController::~SignController() = default;

} // namespace librecelik::agent
