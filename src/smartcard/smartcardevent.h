// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARDEVENT_H
#define SMARTCARDEVENT_H

#include <string>
#include <ostream>
#include <sstream>
#include <QMetaType>
#include <QDebug>

struct SmartCardEvent
{
    std::string readerName;
    typedef enum EventType
    {
        Unknown = 1 << 1,
        CardInserted = 1 << 2,
        CardRemoved = 1 << 3,
    } EventType;
    EventType eventType;
};

inline SmartCardEvent::EventType operator| (SmartCardEvent::EventType a, SmartCardEvent::EventType b)
{
    return static_cast<SmartCardEvent::EventType>(static_cast<int>(a) | static_cast<int>(b));
}

inline SmartCardEvent::EventType operator& (SmartCardEvent::EventType a, SmartCardEvent::EventType b)
{
    return static_cast<SmartCardEvent::EventType>(static_cast<int>(a) & static_cast<int>(b));
}

inline std::ostream& operator<<(std::ostream & os, SmartCardEvent::EventType cat)
{
    switch (cat)
    {
    case SmartCardEvent::Unknown:
    {
        os << "Unknown";
        break;
    }
    case SmartCardEvent::CardInserted:
    {
        os << "CardInserted";
        break;
    }
    case SmartCardEvent::CardRemoved:
    {
        os << "CardRemoved";
        break;
    }
    }
    return os;
}

inline QDebug &operator<<(QDebug & dbg, SmartCardEvent::EventType cat)
{
    std::stringstream eventType;
    eventType << cat;
    dbg.nospace() << eventType.str();
    return dbg.space();
}

Q_DECLARE_METATYPE(SmartCardEvent)

#endif // SMARTCARDEVENT_H
