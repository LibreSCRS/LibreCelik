#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QWidget>
#include <string>

class Document : public QWidget
{
    Q_OBJECT
public:
    virtual ~Document() = default;

    static Document* CreateDocument(const std::string &reader, QWidget* parent = nullptr);

protected:
    Document(QWidget *parent = nullptr);
};

#endif // DOCUMENT_H
