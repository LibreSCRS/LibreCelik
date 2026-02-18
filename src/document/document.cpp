#include "document.h"
#include "document/eid/eid.h"
#include <eidcard/eidcard.h>

Document::Document(QWidget *parent) : QWidget (parent) {}

Document* Document::CreateDocument(const std::string& reader, QWidget *parent)
{
    // Lightweight probe — checks ATR / AID without opening a full session
    if (eidcard::EIdCard::probe(reader))
        return new EId(reader, parent);

    return nullptr;
}
