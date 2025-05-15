#include "document.h"
#include "document/eid/eid.h"

Document::Document(QWidget *parent) : QWidget (parent) {}

Document* Document::CreateDocument(const std::string& reader, QWidget *parent)
{
    // Currently only EID
    return new EId(reader, parent);
}
