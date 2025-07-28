#ifndef EXPRESSIONDOUBLESPINBOX_H
#define EXPRESSIONDOUBLESPINBOX_H

#include <QDoubleSpinBox>

namespace SVS {

    class ExpressionDoubleSpinBox : public QDoubleSpinBox {
        Q_OBJECT
    public:
        explicit ExpressionDoubleSpinBox(QWidget *parent = nullptr);
        ~ExpressionDoubleSpinBox() override;

        QString cleanText() const = delete;

        QString prefix() const = delete;
        void setPrefix(const QString &) = delete;
        QString suffix() const = delete;
        void setSuffix(const QString &) = delete;

        QValidator::State validate(QString &input, int &pos) const override;
        void fixup(QString &str) const override;
    };

}

#endif // EXPRESSIONDOUBLESPINBOX_H
