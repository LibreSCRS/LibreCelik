// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Out-of-line ctor/dtor: the key function that anchors CardController's vtable
// and typeinfo to this translation unit instead of emitting a copy in every
// TU that includes the header.

#include "agent/cardcontroller.h"

namespace librecelik::agent {

CardController::CardController(QObject* parent) : QObject(parent) {}

CardController::~CardController() = default;

} // namespace librecelik::agent
