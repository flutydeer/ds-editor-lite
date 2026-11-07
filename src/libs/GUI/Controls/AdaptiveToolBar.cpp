#include "AdaptiveToolBar.h"
#include "AdaptiveToolBarLayout_p.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ControlGroup.h>
#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMap>
#include <QPointer>
#include <QScreen>
#include <QStyle>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <utility>

namespace AdaptiveToolBarLayoutDetails {

    Plan calculate(const QList<Entry> &entries, int width, const QMap<int, QSize> &moreSizes,
                   int itemSpacing, int groupSpacing) {
        QList<int> display;
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].applicable)
                display.append(i);
        }
        std::stable_sort(display.begin(), display.end(), [&](int a, int b) {
            if (entries[a].group != entries[b].group)
                return entries[a].group < entries[b].group;
            return entries[a].order < entries[b].order;
        });
        QMap<int, QList<int>> groups;
        for (const int index : display)
            groups[entries[index].group].append(index);
        const auto place = [&](const QList<int> &kept) {
            Plan result;
            result.inlineItems = kept;
            result.itemRects.resize(entries.size());
            int x = 0;
            int height = 0;
            for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
                if (it != groups.cbegin())
                    x += groupSpacing;
                const int start = x;
                int groupHeight = 0;
                bool hasInline = false;
                bool hasOverflow = false;
                for (const int index : it.value()) {
                    if (!kept.contains(index)) {
                        result.overflowItems.append(index);
                        hasOverflow = true;
                        continue;
                    }
                    if (hasInline)
                        x += itemSpacing;
                    result.itemRects[index] = QRect(QPoint(x, 0), entries[index].size);
                    x += entries[index].size.width();
                    groupHeight = qMax(groupHeight, entries[index].size.height());
                    hasInline = true;
                }
                if (hasOverflow) {
                    if (hasInline)
                        x += itemSpacing;
                    const auto size = moreSizes.value(it.key());
                    result.moreRects.insert(it.key(), QRect(QPoint(x, 0), size));
                    x += size.width();
                    groupHeight = qMax(groupHeight, size.height());
                }
                result.groupRects.insert(it.key(), QRect(start, 0, x - start, groupHeight));
                height = qMax(height, groupHeight);
            }
            result.fullSize = QSize(x, height);
            return result;
        };
        auto result = place(display);
        const auto fullSize = result.fullSize;
        if (fullSize.width() > width) {
            auto removal = display;
            std::stable_sort(removal.begin(), removal.end(), [&](int a, int b) {
                if (entries[a].priority != entries[b].priority)
                    return entries[a].priority < entries[b].priority;
                return display.indexOf(a) > display.indexOf(b);
            });
            auto kept = display;
            for (const int index : removal) {
                kept.removeOne(index);
                result = place(kept);
                if (result.fullSize.width() <= width)
                    break;
            }
        }
        result.fullSize = fullSize;
        return result;
    }

    Plan calculate(const QList<Entry> &entries, int width, const QSize &moreSize, int itemSpacing,
                   int groupSpacing) {
        QMap<int, QSize> moreSizes;
        for (const auto &entry : entries)
            moreSizes.insert(entry.group, moreSize);
        return calculate(entries, width, moreSizes, itemSpacing, groupSpacing);
    }

}

namespace {

    QSize naturalSize(QWidget *widget) {
        if (!widget)
            return {};
        widget->ensurePolished();
        return widget->sizeHint()
            .expandedTo(widget->minimumSizeHint())
            .expandedTo(widget->minimumSize())
            .expandedTo(QSize(0, 0))
            .boundedTo(widget->maximumSize());
    }

    class AdaptiveLayout final : public QLayout {
    public:
        explicit AdaptiveLayout(QWidget *parent) : QLayout(parent) {
            setContentsMargins(0, 0, 0, 0);
        }

        ~AdaptiveLayout() override {
            while (auto *item = takeAt(0))
                delete item;
        }

        std::function<QSize()> preferred;
        std::function<QSize()> minimum;
        std::function<void(const QRect &)> arrange;

        void addItem(QLayoutItem *item) override {
            items.append(item);
        }

        int count() const override {
            return items.size();
        }

        QLayoutItem *itemAt(int index) const override {
            return items.value(index);
        }

        QLayoutItem *takeAt(int index) override {
            return index >= 0 && index < items.size() ? items.takeAt(index) : nullptr;
        }

        QSize sizeHint() const override {
            return preferred ? preferred() : QSize(0, 0);
        }

        QSize minimumSize() const override {
            return minimum ? minimum() : QSize(0, 0);
        }

        Qt::Orientations expandingDirections() const override {
            return Qt::Horizontal;
        }

        void setGeometry(const QRect &rect) override {
            QLayout::setGeometry(rect);
            if (arrange)
                arrange(rect);
        }

    private:
        QList<QLayoutItem *> items;
    };

    // Wrap only between original controls. A popup never compresses a control or adds scrollbars.
    class PopupFlowLayout final : public QLayout {
    public:
        explicit PopupFlowLayout(QWidget *parent) : QLayout(parent) {
            setContentsMargins(0, 0, 0, 0);
            setSpacing(1);
        }

        ~PopupFlowLayout() override {
            while (auto *item = takeAt(0))
                delete item;
        }

        void addItem(QLayoutItem *item) override {
            items.append(item);
            invalidate();
        }

        void insertWidget(int index, QWidget *widget) {
            const int oldIndex = indexOf(widget);
            if (oldIndex == index)
                return;
            if (oldIndex >= 0)
                delete takeAt(oldIndex);
            addChildWidget(widget);
            items.insert(index, new QWidgetItem(widget));
            invalidate();
        }

        int count() const override {
            return items.size();
        }

        QLayoutItem *itemAt(int index) const override {
            return items.value(index);
        }

        QLayoutItem *takeAt(int index) override {
            return index >= 0 && index < items.size() ? items.takeAt(index) : nullptr;
        }

        QSize sizeHint() const override {
            return measure(QWIDGETSIZE_MAX);
        }

        QSize minimumSize() const override {
            QSize size(0, 0);
            for (auto *item : items) {
                if (auto *widget = item->widget(); widget && !widget->isHidden())
                    size = size.expandedTo(naturalSize(widget));
            }
            return size;
        }

        bool hasHeightForWidth() const override {
            return true;
        }

        int heightForWidth(int width) const override {
            return measure(width).height();
        }

        Qt::Orientations expandingDirections() const override {
            return {};
        }

        QSize measure(int width, const QRect *geometry = nullptr) const {
            int x = 0;
            int y = 0;
            int rowHeight = 0;
            int usedWidth = 0;
            for (auto *item : items) {
                auto *widget = item->widget();
                if (!widget || widget->isHidden())
                    continue;
                const auto size = naturalSize(widget);
                if (x > 0 && x + spacing() + size.width() > width) {
                    y += rowHeight + 4;
                    x = 0;
                    rowHeight = 0;
                }
                if (x > 0)
                    x += spacing();
                if (geometry) {
                    const QRect rect(geometry->topLeft() + QPoint(x, y), size);
                    widget->setGeometry(
                        QStyle::visualRect(parentWidget()->layoutDirection(), *geometry, rect));
                }
                x += size.width();
                usedWidth = qMax(usedWidth, x);
                rowHeight = qMax(rowHeight, size.height());
            }
            return QSize(usedWidth, y + rowHeight);
        }

        void setGeometry(const QRect &rect) override {
            QLayout::setGeometry(rect);
            measure(rect.width(), &rect);
        }

    private:
        QList<QLayoutItem *> items;
    };

}

class AdaptiveToolBarPrivate {
public:
    struct Item {
        QPointer<QWidget> widget;
        AdaptiveToolBar::ItemOptions options;
        bool applicable = true;
        bool overflowed = false;
    };

    struct Group {
        ControlGroup *inlineHost = nullptr;
        QHBoxLayout *inlineLayout = nullptr;
        Button *more = nullptr;
        QFrame *popup = nullptr;
        ControlGroup *popupHost = nullptr;
        PopupFlowLayout *popupLayout = nullptr;
        QPointer<QWidget> oldFocus;
    };

    explicit AdaptiveToolBarPrivate(AdaptiveToolBar *owner) : q(owner) {
    }

    AdaptiveToolBar *q;
    QList<Item> items;
    QMap<int, Group> groups;
    AdaptiveLayout *layout = nullptr;
    QList<QSize> measuredSizes;
    QMap<int, QSize> measuredMoreSizes;
    int itemSpacing = 1;
    int groupSpacing = 18;
    bool applying = false;
    bool pending = false;
    bool destroying = false;

    QList<AdaptiveToolBarLayoutDetails::Entry> entries() const {
        QList<AdaptiveToolBarLayoutDetails::Entry> result;
        for (const auto &item : items) {
            result.append({naturalSize(item.widget), item.options.group, item.options.order,
                           item.options.overflowPriority, item.applicable && item.widget});
        }
        return result;
    }

    QMap<int, QSize> moreSizes() const {
        QMap<int, QSize> sizes;
        for (auto it = groups.cbegin(); it != groups.cend(); ++it)
            sizes.insert(it.key(), naturalSize(it->more));
        return sizes;
    }

    AdaptiveToolBarLayoutDetails::Plan plan(int width) const {
        return AdaptiveToolBarLayoutDetails::calculate(entries(), width, moreSizes(), itemSpacing,
                                                       groupSpacing);
    }

    void requestLayout() {
        if (destroying || pending)
            return;
        pending = true;
        layout->invalidate();
        q->updateGeometry();
        QTimer::singleShot(0, q, [this] {
            pending = false;
            layout->invalidate();
            layout->activate();
        });
    }

    void closeNestedPopups(QWidget *ancestor) {
        // Close only our descendants, never an unrelated application popup.
        for (int depth = 0; depth < 32; ++depth) {
            auto *active = QApplication::activePopupWidget();
            if (!active || active == ancestor || !ancestor->isAncestorOf(active))
                break;
            active->close();
            if (QApplication::activePopupWidget() == active)
                break;
        }
    }

    void closePopups() {
        for (auto &group : groups) {
            closeNestedPopups(group.popup);
            group.popup->hide();
        }
    }

    void createGroup(int id) {
        if (groups.contains(id))
            return;
        Group group;
        group.inlineHost = new ControlGroup(q);
        group.inlineHost->hide();
        group.inlineLayout = new QHBoxLayout(group.inlineHost);
        group.inlineLayout->setContentsMargins(0, 0, 0, 0);
        group.inlineLayout->setSpacing(itemSpacing);
        group.more = new Button(QString(QChar(0x22ef)), group.inlineHost);
        group.more->setObjectName("adaptiveToolBarMoreButton");
        group.more->setProperty("overflowGroup", id);
        group.more->setFixedSize(24, 28);
        group.more->hide();
        group.more->installEventFilter(q);
        group.inlineLayout->addWidget(group.more);
        group.popup = new QFrame(q, Qt::Popup | Qt::NoDropShadowWindowHint);
        group.popup->setObjectName("adaptiveToolBarPopup");
        group.popup->setProperty("overflowGroup", id);
        group.popup->setAttribute(Qt::WA_StyledBackground);
        group.popup->setAttribute(Qt::WA_WindowPropagation);
        group.popup->setLayoutDirection(q->layoutDirection());
        group.popup->installEventFilter(q);
        group.popupHost = new ControlGroup(group.popup);
        group.popupLayout = new PopupFlowLayout(group.popupHost);
        group.popupLayout->setSpacing(itemSpacing);
        const auto popupLayout = new QHBoxLayout(group.popup);
        popupLayout->setContentsMargins(4, 4, 4, 4);
        popupLayout->setSpacing(0);
        popupLayout->setSizeConstraint(QLayout::SetNoConstraint);
        popupLayout->addWidget(group.popupHost);
        group.popupHost->show();
        group.inlineHost->installEventFilter(q);
        group.popupHost->installEventFilter(q);
        layout->addWidget(group.inlineHost);
        groups.insert(id, group);
        QObject::connect(group.more, &Button::clicked, q, [this, id] {
            if (groups[id].popup->isVisible())
                closePopups();
            else
                showPopup(id);
        });
    }

    void arrange(const QRect &rect) {
        if (applying || destroying)
            return;
        applying = true;
        const auto metrics = entries();
        measuredSizes.clear();
        for (const auto &entry : metrics)
            measuredSizes.append(entry.size);
        measuredMoreSizes = moreSizes();
        const auto result = AdaptiveToolBarLayoutDetails::calculate(
            metrics, rect.width(), measuredMoreSizes, itemSpacing, groupSpacing);
        QList<int> relocatingGroups;
        for (int i = 0; i < items.size(); ++i) {
            const auto &item = items[i];
            if (!item.widget || !item.applicable)
                continue;
            const auto &group = groups[item.options.group];
            QWidget *target = result.overflowItems.contains(i) ? group.popupHost : group.inlineHost;
            if (item.widget->parentWidget() != target &&
                !relocatingGroups.contains(item.options.group))
                relocatingGroups.append(item.options.group);
        }
        for (const int id : relocatingGroups) {
            const auto &group = groups[id];
            closeNestedPopups(group.inlineHost);
            closeNestedPopups(group.popup);
            group.popup->hide();
        }
        for (int i = 0; i < items.size(); ++i) {
            auto &item = items[i];
            if (!item.widget)
                continue;
            auto &group = groups[item.options.group];
            item.overflowed = result.overflowItems.contains(i);
            if (!item.applicable) {
                item.widget->hide();
                continue;
            }
            QWidget *target = item.overflowed ? group.popupHost : group.inlineHost;
            if (item.widget->parentWidget() != target) {
                emit q->widgetAboutToRelocate(item.widget, target);
                if (auto *oldLayout = item.widget->parentWidget()->layout())
                    oldLayout->removeWidget(item.widget);
                item.widget->setParent(target);
                if (item.overflowed)
                    group.popupLayout->addWidget(item.widget);
                else
                    group.inlineLayout->addWidget(item.widget);
            }
            item.widget->show();
        }
        // Each group's More is its final member, not a separate trailing toolbar action.
        for (auto it = groups.begin(); it != groups.end(); ++it) {
            auto &group = it.value();
            int inlineIndex = 0;
            int popupIndex = 0;
            for (const int index : result.inlineItems) {
                if (items[index].options.group == it.key()) {
                    const auto *existing = group.inlineLayout->itemAt(inlineIndex);
                    if (!existing || existing->widget() != items[index].widget)
                        group.inlineLayout->insertWidget(inlineIndex, items[index].widget);
                    ++inlineIndex;
                }
            }
            const auto *last = group.inlineLayout->itemAt(inlineIndex);
            if (!last || last->widget() != group.more)
                group.inlineLayout->insertWidget(inlineIndex, group.more);
            for (const int index : result.overflowItems) {
                if (items[index].options.group == it.key())
                    group.popupLayout->insertWidget(popupIndex++, items[index].widget);
            }
            // Hidden hosts do not auto-activate layouts on LayoutRequest. Rearm their layout
            // so each subsequent child updateGeometry() can post another request.
            group.popupLayout->invalidate();
            group.popupLayout->activate();
            const bool overflowing = result.moreRects.contains(it.key());
            group.more->setVisible(overflowing);
            group.inlineHost->setVisible(result.groupRects.contains(it.key()));
            if (const auto r = result.groupRects.value(it.key()); !r.isEmpty()) {
                auto centered = r.translated(rect.topLeft());
                centered.moveTop(rect.y() + qMax(0, (rect.height() - r.height()) / 2));
                group.inlineHost->setGeometry(
                    QStyle::visualRect(q->layoutDirection(), rect, centered));
                group.inlineLayout->invalidate();
                group.inlineLayout->activate();
            }
            if (!overflowing) {
                closeNestedPopups(group.popup);
                group.popup->hide();
            } else if (group.popup->isVisible()) {
                positionPopup(it.key());
            }
        }
        applying = false;
    }

    void positionPopup(int id) {
        auto &group = groups[id];
        const QPoint anchor = group.more->mapToGlobal(QPoint(0, group.more->height()));
        auto *screen = QGuiApplication::screenAt(anchor);
        if (!screen)
            screen = q->screen();
        const QRect available = screen->availableGeometry().adjusted(4, 4, -4, -4);
        group.popup->ensurePolished();
        const auto margins =
            group.popup->contentsMargins() + group.popup->layout()->contentsMargins();
        const QSize padding(margins.left() + margins.right(), margins.top() + margins.bottom());
        const QSize content =
            group.popupLayout->measure(qMax(1, available.width() - padding.width()));
        const QSize size = content + padding;
        group.popup->setFixedSize(size);
        group.popup->layout()->invalidate();
        group.popup->layout()->activate();
        group.popupLayout->invalidate();
        group.popupLayout->activate();
        QPoint position = anchor;
        if (q->layoutDirection() == Qt::RightToLeft)
            position.rx() += group.more->width() - size.width();
        if (position.y() + size.height() > available.bottom() + 1)
            position.setY(group.more->mapToGlobal(QPoint(0, 0)).y() - size.height());
        position.setX(
            qMax(available.left(), qMin(position.x(), available.right() - size.width() + 1)));
        position.setY(
            qMax(available.top(), qMin(position.y(), available.bottom() - size.height() + 1)));
        group.popup->move(position);
    }

    void showPopup(int id) {
        layout->activate();
        auto &group = groups[id];
        if (group.more->isHidden())
            return;
        auto *focus = QApplication::focusWidget();
        // Switching groups inherits the original main-window focus, not a closing popup child.
        for (const auto &other : std::as_const(groups)) {
            if (other.popup->isVisible() && focus && other.popup->isAncestorOf(focus)) {
                focus = other.oldFocus;
                break;
            }
        }
        closePopups();
        group.oldFocus = focus;
        positionPopup(id);
        group.popup->show();
        group.popup->raise();
        WindowFrameUtils::applyPopupEffects(group.popup);
        for (int i = 0; i < group.popupLayout->count(); ++i) {
            auto *widget = group.popupLayout->itemAt(i)->widget();
            if (widget && !widget->isHidden() && widget->focusPolicy() != Qt::NoFocus &&
                widget->isEnabled()) {
                widget->setFocus(Qt::PopupFocusReason);
                break;
            }
        }
    }
};

AdaptiveToolBar::AdaptiveToolBar(QWidget *parent)
    : QWidget(parent), d(std::make_unique<AdaptiveToolBarPrivate>(this)) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    d->layout = new AdaptiveLayout(this);
    d->layout->preferred = [this] { return sizeHint(); };
    d->layout->minimum = [this] { return minimumSizeHint(); };
    d->layout->arrange = [this](const QRect &rect) { d->arrange(rect); };
    d->createGroup(0);
}

AdaptiveToolBar::~AdaptiveToolBar() {
    d->destroying = true;
    d->closePopups();
    // QWidget destroys children after members, so detach callbacks before private state dies.
    d->layout->preferred = {};
    d->layout->minimum = {};
    d->layout->arrange = {};
    for (const auto &item : std::as_const(d->items)) {
        if (item.widget) {
            item.widget->removeEventFilter(this);
            disconnect(item.widget, nullptr, this, nullptr);
        }
    }
    for (const auto &group : std::as_const(d->groups)) {
        group.inlineHost->removeEventFilter(this);
        group.popupHost->removeEventFilter(this);
        group.more->removeEventFilter(this);
        group.popup->removeEventFilter(this);
    }
}

void AdaptiveToolBar::addWidget(QWidget *widget, const ItemOptions &options) {
    if (!widget)
        return;
    for (const auto &item : std::as_const(d->items)) {
        if (item.widget == widget)
            return;
    }
    d->createGroup(options.group);
    auto &group = d->groups[options.group];
    widget->hide();
    emit widgetAboutToRelocate(widget, group.inlineHost);
    widget->setParent(group.inlineHost);
    group.inlineLayout->addWidget(widget);
    d->items.append({widget, options, true, false});
    widget->installEventFilter(this);
    connect(widget, &QObject::destroyed, this, [this] { d->requestLayout(); });
    d->requestLayout();
}

void AdaptiveToolBar::setItemApplicable(QWidget *widget, bool applicable) {
    for (auto &item : d->items) {
        if (item.widget == widget && item.applicable != applicable) {
            item.applicable = applicable;
            // Context changes must immediately hide controls that no longer apply.
            if (!applicable && item.widget)
                item.widget->hide();
            d->requestLayout();
            return;
        }
    }
}

bool AdaptiveToolBar::isItemApplicable(QWidget *widget) const {
    for (const auto &item : d->items) {
        if (item.widget == widget)
            return item.applicable;
    }
    return false;
}

bool AdaptiveToolBar::isWidgetOverflowed(QWidget *widget) const {
    for (const auto &item : d->items) {
        if (item.widget == widget)
            return item.applicable && item.overflowed;
    }
    return false;
}

void AdaptiveToolBar::invalidateItemSize(QWidget *widget) {
    Q_UNUSED(widget);
    d->requestLayout();
}

void AdaptiveToolBar::setItemSpacing(int spacing) {
    d->itemSpacing = qMax(0, spacing);
    for (auto &group : d->groups) {
        group.inlineLayout->setSpacing(d->itemSpacing);
        group.popupLayout->setSpacing(d->itemSpacing);
    }
    d->requestLayout();
}

void AdaptiveToolBar::setGroupSpacing(int spacing) {
    d->groupSpacing = qMax(0, spacing);
    d->requestLayout();
}

Button *AdaptiveToolBar::overflowButton(int group) const {
    const auto it = d->groups.constFind(group);
    return it == d->groups.cend() ? nullptr : it->more;
}

QWidget *AdaptiveToolBar::overflowPopup(int group) const {
    const auto it = d->groups.constFind(group);
    return it == d->groups.cend() ? nullptr : it->popup;
}

void AdaptiveToolBar::closeOverflow() {
    d->closePopups();
}

QSize AdaptiveToolBar::sizeHint() const {
    return d->plan(QWIDGETSIZE_MAX).fullSize;
}

QSize AdaptiveToolBar::minimumSizeHint() const {
    QMap<int, QSize> sizes;
    for (const auto &item : std::as_const(d->items)) {
        if (item.widget && item.applicable)
            sizes.insert(item.options.group, naturalSize(d->groups[item.options.group].more));
    }
    QSize result(0, 0);
    for (auto it = sizes.cbegin(); it != sizes.cend(); ++it) {
        if (it != sizes.cbegin())
            result.rwidth() += d->groupSpacing;
        result.rwidth() += it.value().width();
        result.setHeight(qMax(result.height(), it.value().height()));
    }
    return result;
}

bool AdaptiveToolBar::event(QEvent *event) {
    const bool result = QWidget::event(event);
    if (d && event->type() == QEvent::LayoutDirectionChange) {
        // Qt does not propagate per-widget direction across child-window boundaries.
        for (auto &group : d->groups)
            group.popup->setLayoutDirection(layoutDirection());
        d->requestLayout();
    }
    if (d && (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange ||
              event->type() == QEvent::LanguageChange ||
              event->type() == QEvent::DevicePixelRatioChange))
        d->requestLayout();
    if (d && event->type() == QEvent::ParentAboutToChange)
        closeOverflow();
    return result;
}

bool AdaptiveToolBar::eventFilter(QObject *watched, QEvent *event) {
    for (auto it = d->groups.begin(); it != d->groups.end(); ++it) {
        auto &group = it.value();
        if (watched != group.popup)
            continue;
        if (event->type() == QEvent::MouseButtonPress) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (!group.popup->rect().contains(mouse->position().toPoint()) &&
                group.more->rect().contains(
                    group.more->mapFromGlobal(mouse->globalPosition().toPoint()))) {
                // Suppress replay only for this anchor. A different group's More must open.
                group.popup->setAttribute(Qt::WA_NoMouseReplay);
            }
        } else if (event->type() == QEvent::Hide) {
            group.more->setDown(false);
            if (!d->destroying) {
                const int id = it.key();
                QTimer::singleShot(0, this, [this, id] {
                    if (!isVisible() || QApplication::activePopupWidget() ||
                        QApplication::activeWindow() != window())
                        return;
                    const auto &closed = d->groups[id];
                    const auto *focus = QApplication::focusWidget();
                    if (focus && focus != closed.popup && !closed.popup->isAncestorOf(focus))
                        return;
                    auto *target = closed.oldFocus.data();
                    if (!target || !target->isVisible() || !target->isEnabled())
                        target = closed.more;
                    if (target->isVisible() && target->isEnabled())
                        target->setFocus(Qt::PopupFocusReason);
                });
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape) {
                d->closeNestedPopups(group.popup);
                group.popup->hide();
                return true;
            }
        }
        break;
    }
    if (event->type() == QEvent::LayoutRequest) {
        // Child updateGeometry() targets its group, including when the popup is closed.
        if (!d->applying) {
            // Even a no-op hidden hint invalidation must rearm its layout. Otherwise the
            // next updateGeometry() would not post a LayoutRequest at all.
            for (const auto &group : std::as_const(d->groups)) {
                if (watched == group.popupHost && !group.popupHost->isVisible()) {
                    group.popupLayout->activate();
                    break;
                }
            }
            QList<QSize> sizes;
            for (const auto &entry : d->entries())
                sizes.append(entry.size);
            if (sizes != d->measuredSizes || d->moreSizes() != d->measuredMoreSizes)
                d->requestLayout();
        }
    } else if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange ||
               event->type() == QEvent::LanguageChange ||
               event->type() == QEvent::DevicePixelRatioChange) {
        d->requestLayout();
    }
    return QWidget::eventFilter(watched, event);
}

void AdaptiveToolBar::hideEvent(QHideEvent *event) {
    closeOverflow();
    QWidget::hideEvent(event);
}
