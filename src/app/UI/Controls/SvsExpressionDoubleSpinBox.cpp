#include "SvsExpressionDoubleSpinBox.h"

#include "UI/Utils/IconUtils.h"

#include <QPainter>
#include <QStyleOptionSpinBox>
#include <tinyexpr.h>

namespace SVS {

    ExpressionDoubleSpinBox::ExpressionDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {
    }

    ExpressionDoubleSpinBox::~ExpressionDoubleSpinBox() = default;

    void ExpressionDoubleSpinBox::paintEvent(QPaintEvent *event) {
        QDoubleSpinBox::paintEvent(event);

        QStyleOptionSpinBox option;
        initStyleOption(&option);

        QPainter painter(this);
        const QSize iconSize(16, 16);
        const QColor iconColor = option.palette.color(isEnabled() ? QPalette::Active
                                                                  : QPalette::Disabled,
                                                      QPalette::ButtonText);
        auto drawArrow = [&](QStyle::SubControl control, const QString &iconPath) {
            const QRect buttonRect =
                style()->subControlRect(QStyle::CC_SpinBox, &option, control, this);
            if (buttonRect.isEmpty())
                return;
            painter.fillRect(buttonRect, option.palette.brush(QPalette::Button));
            const QPoint iconPos(buttonRect.x() + (buttonRect.width() - iconSize.width()) / 2,
                                 buttonRect.y() + (buttonRect.height() - iconSize.height()) / 2);
            const auto icon =
                IconUtils::createTintedSvgIcon(iconPath, iconSize, iconColor, iconColor);
            painter.drawPixmap(iconPos, icon.pixmap(iconSize, isEnabled() ? QIcon::Normal
                                                                          : QIcon::Disabled));
        };

        drawArrow(QStyle::SC_SpinBoxUp,
                  QStringLiteral(":/svg/icons/chevron_right_16_filled.svg"));
        drawArrow(QStyle::SC_SpinBoxDown,
                  QStringLiteral(":/svg/icons/chevron_left_16_filled.svg"));
    }

    QValidator::State ExpressionDoubleSpinBox::validate(QString &input, int &pos) const {
        if (textFromValue(valueFromText(input)) == input)
            return QValidator::Acceptable;
        return QValidator::Intermediate;
    }

    void ExpressionDoubleSpinBox::fixup(QString &str) const {
        int err;
        auto s = str;
        for (auto &c : s) {
            if (c.unicode() >= 0xff01 && c.unicode() <= 0xff5e) {
                c.unicode() -= 0xfee0;
            }
        }
        s.replace(QLocale().decimalPoint(), ".");
        const double ret = te_interp(s.toLatin1(), &err);
        if (err == 0) {
            str = textFromValue(ret);
        }
    }

}
