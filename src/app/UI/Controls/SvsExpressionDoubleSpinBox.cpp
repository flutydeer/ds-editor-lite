#include "SvsExpressionDoubleSpinBox.h"

#include <tinyexpr.h>

namespace SVS {

    ExpressionDoubleSpinBox::ExpressionDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {
    }

    ExpressionDoubleSpinBox::~ExpressionDoubleSpinBox() = default;

    QValidator::State ExpressionDoubleSpinBox::validate(QString &input, int &pos) const {
        if (textFromValue(valueFromText(input)) == input)
            return QValidator::Acceptable;
        else
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
        double ret = te_interp(s.toLatin1(), &err);
        if (err == 0) {
            str = textFromValue(ret);
        }
    }

}