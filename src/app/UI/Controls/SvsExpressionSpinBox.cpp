#include "SvsExpressionSpinBox.h"

#include "UI/Controls/Menu.h"
#include "UI/Utils/IconUtils.h"

#include <QContextMenuEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <tinyexpr.h>

#include <array>

namespace {
    constexpr std::array<const char *, 7> kStandardActionIcons = {
        ":/svg/icons/arrow_undo_16_regular.svg",
        ":/svg/icons/arrow_redo_16_regular.svg",
        ":/svg/icons/cut_16_regular.svg",
        ":/svg/icons/copy_16_regular.svg",
        ":/svg/icons/clipboard_paste_16_regular.svg",
        ":/svg/icons/delete_16_regular.svg",
        ":/svg/icons/select_all_on_16_regular.svg",
    };
}

namespace SVS {

    ExpressionSpinBox::ExpressionSpinBox(QWidget *parent) : QSpinBox(parent) {
    }

    ExpressionSpinBox::~ExpressionSpinBox() = default;

    void ExpressionSpinBox::paintEvent(QPaintEvent *event) {
        QSpinBox::paintEvent(event);

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

    void ExpressionSpinBox::mousePressEvent(QMouseEvent *event) {
        QSpinBox::mousePressEvent(event);
        event->ignore();
    }

    Menu *ExpressionSpinBox::createContextMenu(QWidget *parent) {
        if (!lineEdit())
            return nullptr;

        const auto standardMenu = lineEdit()->createStandardContextMenu();
        if (!standardMenu)
            return nullptr;

        auto *menu = new Menu(parent ? parent : this);
        qsizetype actionIndex = 0;
        for (const auto action : standardMenu->actions()) {
            action->setParent(menu);
            if (!action->isSeparator() && actionIndex < kStandardActionIcons.size()) {
                action->setIcon(
                    IconUtils::menuIcon(QString::fromLatin1(kStandardActionIcons.at(actionIndex))));
                ++actionIndex;
            }
            menu->addAction(action);
        }
        delete standardMenu;

        menu->addSeparator();

        auto *stepUpAction = menu->addAction(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/chevron_up_16_regular.svg")),
            tr("Step Up"));
        connect(stepUpAction, &QAction::triggered, this, &ExpressionSpinBox::stepUp);

        auto *stepDownAction = menu->addAction(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/chevron_down_16_regular.svg")),
            tr("Step Down"));
        connect(stepDownAction, &QAction::triggered, this, &ExpressionSpinBox::stepDown);

        return menu;
    }

    void ExpressionSpinBox::contextMenuEvent(QContextMenuEvent *event) {
        if (const auto menu = createContextMenu(this)) {
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->popup(event->globalPos());
        }
        event->accept();
    }

}
