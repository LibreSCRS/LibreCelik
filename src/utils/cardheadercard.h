// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QWidget>

#include <vector>

class QGridLayout;
class QIcon;
class QLabel;
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
    // Icon variant (Vehicle, Health, PKS)
    explicit CardHeaderCard(const QIcon& icon, const QSize& iconSize, const std::vector<HeaderField>& fields,
                            QWidget* parent = nullptr);

private:
    void buildLayout(QWidget* leftWidget, const std::vector<HeaderField>& fields);
    QGridLayout* fieldsGrid = nullptr;
    QVBoxLayout* rightColumnLayout = nullptr;
};

} // namespace LibreSCRS
