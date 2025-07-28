#ifndef EXPRESSIONSPINBOX_H
#define EXPRESSIONSPINBOX_H

#include <QSpinBox>

namespace SVS {

    class ExpressionSpinBox : public QSpinBox {
        Q_OBJECT
    public:
        explicit ExpressionSpinBox(QWidget *parent = nullptr);
        ~ExpressionSpinBox() override;

        QString cleanText() const = delete;

        int displayIntegerBase() const = delete;
        void setDisplayIntegerBase(int) = delete;

        QString prefix() const = delete;
        void setPrefix(const QString &) = delete;
        QString suffix() const = delete;
        void setSuffix(const QString &) = delete;

    protected:
        QValidator::State validate(QString &input, int &pos) const override;
        void fixup(QString &str) const override;
    };

}

#endif // EXPRESSIONSPINBOX_H
