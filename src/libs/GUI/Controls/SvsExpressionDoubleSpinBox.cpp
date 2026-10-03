#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Utils/IconUtils.h>

#include <QContextMenuEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <tinyexpr.h>

namespace SVS {

    ExpressionDoubleSpinBox::ExpressionDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent) {
        setFixedHeight(28);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    ExpressionDoubleSpinBox::~ExpressionDoubleSpinBox() = default;

    void ExpressionDoubleSpinBox::paintEvent(QPaintEvent *event) {
        QDoubleSpinBox::paintEvent(event);

        QStyleOptionSpinBox option;
        initStyleOption(&option);

        QPainter painter(this);
        const QSize iconSize(16, 16);
        const QColor iconColor = option.palette.color(
            isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
        const qreal dpr = devicePixelRatioF();
        auto drawArrow = [&](QStyle::SubControl control, const QString &iconPath) {
            const QRect buttonRect =
                style()->subControlRect(QStyle::CC_SpinBox, &option, control, this);
            if (buttonRect.isEmpty())
                return;
            const QPoint iconPos(buttonRect.x() + (buttonRect.width() - iconSize.width()) / 2,
                                 buttonRect.y() + (buttonRect.height() - iconSize.height()) / 2);
            const auto pixmap =
                IconUtils::renderTintedSvgPixmap(iconPath, iconSize, iconColor, dpr);
            painter.drawPixmap(iconPos, pixmap);
        };

        drawArrow(QStyle::SC_SpinBoxUp, QStringLiteral(":/svg/icons/chevron_up_16_regular.svg"));
        drawArrow(QStyle::SC_SpinBoxDown,
                  QStringLiteral(":/svg/icons/chevron_down_16_regular.svg"));
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

    void ExpressionDoubleSpinBox::mousePressEvent(QMouseEvent *event) {
        QDoubleSpinBox::mousePressEvent(event);
        event->ignore();
    }

    Menu *ExpressionDoubleSpinBox::createContextMenu(QWidget *parent) {
        auto *menu = Menu::fromLineEdit(lineEdit(), parent ? parent : this);
        if (!menu)
            return nullptr;

        menu->addSeparator();

        auto *stepUpAction = menu->addAction(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/chevron_up_16_regular.svg")),
            tr("Step Up"));
        connect(stepUpAction, &QAction::triggered, this, &ExpressionDoubleSpinBox::stepUp);

        auto *stepDownAction = menu->addAction(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/chevron_down_16_regular.svg")),
            tr("Step Down"));
        connect(stepDownAction, &QAction::triggered, this, &ExpressionDoubleSpinBox::stepDown);

        return menu;
    }

    void ExpressionDoubleSpinBox::contextMenuEvent(QContextMenuEvent *event) {
        if (const auto menu = createContextMenu(this)) {
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->popup(event->globalPos());
        }
        event->accept();
    }

    void ExpressionDoubleSpinBox::wheelEvent(QWheelEvent *event) {
        if (processWheelEventPolicy(this, event))
            return;
        QDoubleSpinBox::wheelEvent(event);
    }

}
