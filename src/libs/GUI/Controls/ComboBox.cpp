#include <QWheelEvent>
#include <QStyledItemDelegate>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QLineEdit>
#include <QPainter>
#include <QStyleOptionComboBox>

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>
#include <lite/GUI/Controls/SmoothScroller.h>
#include <lite/GUI/Utils/IconUtils.h>
#include <lite/Support/SystemUtils.h>

ComboBox::ComboBox(QWidget *parent) : ComboBox(WheelEventPolicy::Consume, parent) {
}

ComboBox::ComboBox(const WheelEventPolicy wheelEventPolicy, QWidget *parent)
    : CComboBox(parent), WheelEventPolicySupport(wheelEventPolicy) {
    initUi();
}

void ComboBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QStyleOptionComboBox option;
    initStyleOption(&option);

    QPainter painter(this);

    auto frameOption = option;
    frameOption.subControls &= ~QStyle::SC_ComboBoxArrow;
    style()->drawComplexControl(QStyle::CC_ComboBox, &frameOption, &painter, this);
    style()->drawControl(QStyle::CE_ComboBoxLabel, &option, &painter, this);

    QRect arrowRect =
        style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, this);
    if (arrowRect.isEmpty())
        arrowRect = QRect(width() - 28, 0, 28, height());

    const QSize iconSize(16, 16);
    const QPoint iconPos(arrowRect.x() + (arrowRect.width() - iconSize.width()) / 2,
                         arrowRect.y() + (arrowRect.height() - iconSize.height()) / 2);
    const QColor iconColor = option.palette.color(
        isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
    const auto pixmap =
        IconUtils::renderTintedSvgPixmap(QStringLiteral(":/svg/icons/chevron_down_16_regular.svg"),
                                         iconSize, iconColor, devicePixelRatioF());
    painter.drawPixmap(iconPos, pixmap);
}

void ComboBox::wheelEvent(QWheelEvent *event) {
    if (processWheelEventPolicy(this, event))
        return;
    QComboBox::wheelEvent(event);
}

Menu *ComboBox::createContextMenu(QWidget *parent) {
    return Menu::fromLineEdit(lineEdit(), parent ? parent : this);
}

void ComboBox::contextMenuEvent(QContextMenuEvent *event) {
    // The embedded line edit of an editable combo box has Qt::NoContextMenu policy,
    // so its context menu events always arrive here; show the app-styled menu instead
    // of letting QComboBox forward the event to the native line edit menu
    if (const auto menu = createContextMenu(this)) {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(event->globalPos());
        event->accept();
        return;
    }
    CComboBox::contextMenuEvent(event);
}

void ComboBox::initUi() {
    const auto styledItemDelegate = new QStyledItemDelegate();
    setItemDelegate(styledItemDelegate);

    auto *container = dynamic_cast<QWidget *>(view()->parent());
    container->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    container->setAttribute(Qt::WA_TranslucentBackground, true);
    container->setAttribute(Qt::WA_WindowPropagation);
    container->setAttribute(Qt::WA_X11NetWmWindowTypeCombo);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11()) {
        setProperty("dwmBorder", true);
    }
#endif

    // 弹出层复用 Overlay 滚动条（数据源 = QListView，几何宿主 = 弹出层容器，
    // 使滚动条钉在容器右壁而不是跟着视口内缩）
    if (const auto bar = OverlayScrollBar::install(view(), Qt::Vertical); bar)
        bar->setGeometryHost(container);

    // Smooth-scroll the popup list: animate mouse-wheel with OutCubic, touchpad passes through.
    // SmoothScroller attaches to view()'s viewport, reusing the same view as OverlayScrollBar.
    auto *smoothScroller = new SmoothScroller(this);
    smoothScroller->attachTo(view());
}
