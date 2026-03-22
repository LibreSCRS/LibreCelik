// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QWidget>

#include <vector>

class QGridLayout;
class QIcon;
class QPixmap;
class QVBoxLayout;

namespace LibreSCRS {

struct HeaderField
{
    QString label;
    QString value;
    int columnSpan = 1; // 1 = single column, 2 = spans both columns
};

struct VerificationStatus
{
    QString label;
    enum Result { Unknown, Valid, Invalid } result;
};

class CardHeaderCard : public QWidget
{
    Q_OBJECT
public:
    // Photo variant (eID, eMRTD)
    explicit CardHeaderCard(const QPixmap& photo, const QSize& photoSize, const std::vector<HeaderField>& fields,
                            QWidget* parent = nullptr);

    // Icon variant (Vehicle, Health, PKS)
    explicit CardHeaderCard(const QIcon& icon, const QSize& iconSize, const std::vector<HeaderField>& fields,
                            QWidget* parent = nullptr);

    // Add verification indicators below the fields on the right side.
    void setVerificationResults(const std::vector<VerificationStatus>& results);

private:
    void buildLayout(QWidget* leftWidget, const std::vector<HeaderField>& fields);
    QGridLayout* fieldsGrid = nullptr;
    QVBoxLayout* rightColumnLayout = nullptr;
};

} // namespace LibreSCRS
