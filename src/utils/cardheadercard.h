// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QEvent>
#include <QWidget>

#include <vector>

class QGridLayout;
class QIcon;
class QLabel;
class QPixmap;
class QVBoxLayout;

namespace librecelik::utils {

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

protected:
    void changeEvent(QEvent* event) override;

private:
    void buildLayout(QWidget* leftWidget, const std::vector<HeaderField>& fields);
    void applyLabelStyles();
    QGridLayout* fieldsGrid = nullptr;
    QVBoxLayout* rightColumnLayout = nullptr;
};

} // namespace librecelik::utils
