#include "SvsExpressionSpinBox.h"

#include <tinyexpr.h>

namespace SVS {

    ExpressionSpinBox::ExpressionSpinBox(QWidget *parent) : QSpinBox(parent) {
    }

    ExpressionSpinBox::~ExpressionSpinBox() = default;

    QValidator::State ExpressionSpinBox::validate(QString &input, int &pos) const {
        if (textFromValue(valueFromText(input)) == input)
            return QValidator::Acceptable;
        return QValidator::Intermediate;
    }

    void ExpressionSpinBox::fixup(QString &str) const {
        int err;
        auto s = str;
        for (auto &c : s) {
            if (c.unicode() >= 0xff01 && c.unicode() <= 0xff5e) {
                c.unicode() -= 0xfee0;
            }
        }
        s.replace(QLocale().decimalPoint(), ".");
        const double ret = te_interp(s.toUtf8(), &err);
        if (err == 0) {
            str = textFromValue(static_cast<int>(ret));
        }
    }
}