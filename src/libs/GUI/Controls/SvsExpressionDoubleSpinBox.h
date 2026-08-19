#ifndef EXPRESSIONDOUBLESPINBOX_H
#define EXPRESSIONDOUBLESPINBOX_H

#include <lite/GUI/Controls/WheelEventPolicy.h>

#include <QDoubleSpinBox>

class Menu;

namespace SVS {

    class ExpressionDoubleSpinBox : public QDoubleSpinBox, public WheelEventPolicySupport {
        Q_OBJECT
    public:
        explicit ExpressionDoubleSpinBox(QWidget *parent = nullptr);
        ~ExpressionDoubleSpinBox() override;

        QString cleanText() const = delete;

        QString prefix() const = delete;
        void setPrefix(const QString &) = delete;
        QString suffix() const = delete;
        void setSuffix(const QString &) = delete;

        [[nodiscard]] Menu *createContextMenu(QWidget *parent = nullptr);

        QValidator::State validate(QString &input, int &pos) const override;
        void fixup(QString &str) const override;

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
    };

}

#endif // EXPRESSIONDOUBLESPINBOX_H
