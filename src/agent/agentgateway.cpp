// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Out-of-line ctor/dtor: the key function that anchors AgentGateway's vtable
// and typeinfo to this translation unit instead of emitting a copy in every
// TU that includes the header.

#include "agent/agentgateway.h"

namespace librecelik::agent {

AgentGateway::AgentGateway(QObject* parent) : QObject(parent) {}

AgentGateway::~AgentGateway() = default;

} // namespace librecelik::agent
