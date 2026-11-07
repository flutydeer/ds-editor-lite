#include "tst_gui_components.h"

#include <lite/GUI/Controls/AdaptiveToolBar.h>
#include "../../libs/GUI/Controls/AdaptiveToolBarLayout_p.h"
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/ControlGroup.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QLineEdit>
#include <QPointer>
#include <QScreen>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QWindow>
#include <QtTest/QTest>

#include <memory>

namespace {
    class NaturalWidget final : public QWidget {
    public:
        explicit NaturalWidget(int width) : m_width(width) {
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        }

        QSize sizeHint() const override {
            return {m_width, 28};
        }

        QSize minimumSizeHint() const override {
            return {0, 28};
        }

        void setNaturalWidth(int width) {
            m_width = width;
            updateGeometry();
        }

    private:
        int m_width;
    };

    void settleLayout() {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }

    bool showTestWindow(QWidget &root) {
        root.show();
        if (!QTest::qWaitForWindowExposed(&root))
            return false;
        if (QGuiApplication::platformName() != QStringLiteral("offscreen"))
            root.raise();
        root.activateWindow();
        if (QGuiApplication::platformName() != QStringLiteral("offscreen") &&
            !QTest::qWaitForWindowActive(&root))
            return false;
        settleLayout();
        return true;
    }

    int overflowMask(AdaptiveToolBar &toolbar, const QList<QWidget *> &items) {
        int mask = 0;
        for (int i = 0; i < items.size(); ++i) {
            if (toolbar.isWidgetOverflowed(items.at(i)))
                mask |= 1 << i;
        }
        return mask;
    }
}

void GuiComponentTests::adaptiveToolbarSolverExactFit_data() {
    QTest::addColumn<int>("width");
    QTest::addColumn<QList<int>>("inlineItems");
    QTest::newRow("full-plus-one") << 193 << QList<int>{0, 1, 2};
    QTest::newRow("full-exact") << 192 << QList<int>{0, 1, 2};
    QTest::newRow("full-minus-one") << 191 << QList<int>{1, 2};
    QTest::newRow("second-plus-one") << 133 << QList<int>{1, 2};
    QTest::newRow("second-exact") << 132 << QList<int>{1, 2};
    QTest::newRow("second-minus-one") << 131 << QList<int>{2};
    QTest::newRow("last-plus-one") << 71 << QList<int>{2};
    QTest::newRow("last-exact") << 70 << QList<int>{2};
    QTest::newRow("last-minus-one") << 69 << QList<int>{};
    QTest::newRow("minimum-plus-one") << 51 << QList<int>{};
    QTest::newRow("minimum-exact") << 50 << QList<int>{};
}

void GuiComponentTests::adaptiveToolbarSolverExactFit() {
    QFETCH(int, width);
    QFETCH(QList<int>, inlineItems);
    using namespace AdaptiveToolBarLayoutDetails;
    const QList<Entry> entries{
        {QSize(80, 28), 0, 0, 0, true},
        {QSize(60, 28), 0, 1, 1, true},
        {QSize(40, 28), 1, 0, 2, true}
    };
    const auto plan = calculate(entries, width, QSize(20, 28), 2, 10);
    QCOMPARE(plan.fullSize, QSize(192, 28));
    QCOMPARE(plan.inlineItems, inlineItems);
    QList<int> overflowItems;
    for (int i = 0; i < entries.size(); ++i) {
        if (!inlineItems.contains(i))
            overflowItems.append(i);
        else
            QCOMPARE(plan.itemRects.at(i).size(), entries.at(i).size);
    }
    QCOMPARE(plan.overflowItems, overflowItems);
    QCOMPARE(plan.moreRects.isEmpty(), overflowItems.isEmpty());
    QCOMPARE(plan.groupRects.keys(), QList<int>({0, 1}));
    for (const int group : {0, 1}) {
        bool hasOverflow = false;
        QRect inlineRect;
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).group != group)
                continue;
            if (overflowItems.contains(i))
                hasOverflow = true;
            else
                inlineRect = inlineRect.united(plan.itemRects.at(i));
        }
        QCOMPARE(plan.moreRects.contains(group), hasOverflow);
        const auto groupRect = plan.groupRects.value(group);
        QVERIFY(groupRect.left() >= 0);
        QVERIFY(groupRect.right() < width);
        if (hasOverflow) {
            const auto moreRect = plan.moreRects.value(group);
            QCOMPARE(moreRect.size(), QSize(20, 28));
            QVERIFY(groupRect.contains(moreRect));
            QCOMPARE(moreRect.right(), groupRect.right());
            if (!inlineRect.isEmpty())
                QCOMPARE(moreRect.left() - inlineRect.right() - 1, 2);
        }
    }
    QCOMPARE(plan.groupRects.value(1).left() - plan.groupRects.value(0).right() - 1, 10);
    auto inapplicable = entries;
    for (auto &entry : inapplicable)
        entry.applicable = false;
    const auto empty = calculate(inapplicable, width, QSize(20, 28), 2, 10);
    QVERIFY(empty.inlineItems.isEmpty());
    QVERIFY(empty.overflowItems.isEmpty());
    QVERIFY(empty.moreRects.isEmpty());
    QVERIFY(empty.groupRects.isEmpty());
    QCOMPARE(empty.fullSize, QSize(0, 0));

    const QMap<int, QSize> moreSizes{
        {0, QSize(20, 28)},
        {1, QSize(30, 28)}
    };
    const auto differentButtons = calculate(entries, 60, moreSizes, 2, 10);
    QVERIFY(differentButtons.inlineItems.isEmpty());
    QCOMPARE(differentButtons.moreRects.value(0).size(), QSize(20, 28));
    QCOMPARE(differentButtons.moreRects.value(1).size(), QSize(30, 28));
    QCOMPARE(differentButtons.groupRects.value(1).right(), 59);

    auto singleGroup = entries;
    singleGroup[2].applicable = false;
    const auto onlyFirstGroup = calculate(singleGroup, 20, QSize(20, 28), 2, 10);
    QVERIFY(onlyFirstGroup.inlineItems.isEmpty());
    QCOMPARE(onlyFirstGroup.moreRects.keys(), QList<int>{0});
    QCOMPARE(onlyFirstGroup.groupRects.keys(), QList<int>{0});
    QCOMPARE(onlyFirstGroup.groupRects.value(0).size(), QSize(20, 28));
}

void GuiComponentTests::adaptiveToolbarThresholdsKeepNaturalSizesAndIdentity() {
    // Only the child toolbar is narrow or wider than a screen, never its native owner.
    QWidget root;
    root.resize(640, 120);
    AdaptiveToolBar toolbar(&root);
    toolbar.move(12, 12);
    toolbar.setItemSpacing(3);
    toolbar.setGroupSpacing(17);
    auto *low = new NaturalWidget(79);
    auto *middle = new NaturalWidget(67);
    auto *high = new NaturalWidget(53);
    auto *highest = new NaturalWidget(43);
    // Deliberately register out of presentation order and priority order.
    toolbar.addWidget(highest, {1, 1, 40});
    toolbar.addWidget(middle, {0, 1, 20});
    toolbar.addWidget(high, {1, 0, 30});
    toolbar.addWidget(low, {0, 0, 10});
    const QList<QWidget *> items{low, middle, high, highest};
    const int wide = toolbar.sizeHint().width() + 200;
    toolbar.resize(wide, 36);
    QVERIFY(showTestWindow(root));
    QCOMPARE(overflowMask(toolbar, items), 0);
    QVERIFY(toolbar.overflowButton());
    QVERIFY(toolbar.overflowPopup());
    QCOMPARE(toolbar.overflowButton()->objectName(), QStringLiteral("adaptiveToolBarMoreButton"));
    QCOMPARE(toolbar.overflowPopup()->objectName(), QStringLiteral("adaptiveToolBarPopup"));
    QVERIFY(!toolbar.overflowButton()->isVisible());
    QList<QWidget *> inlineHosts;
    QList<QPointer<QWidget>> popupHosts(4);
    for (auto *item : items) {
        inlineHosts.append(item->parentWidget());
        QVERIFY(qobject_cast<ControlGroup *>(item->parentWidget()));
        QVERIFY(item->width() >= item->sizeHint().width());
    }
    QCOMPARE(inlineHosts.at(0), inlineHosts.at(1));
    QCOMPARE(inlineHosts.at(2), inlineHosts.at(3));
    QVERIFY(inlineHosts.at(0) != inlineHosts.at(2));
    for (const int group : {0, 1}) {
        QVERIFY(toolbar.overflowButton(group));
        QVERIFY(toolbar.overflowPopup(group));
        QCOMPARE(toolbar.overflowButton(group)->parentWidget(), inlineHosts.at(group * 2));
        QVERIFY(!toolbar.overflowButton(group)->isVisible());
    }
    QVERIFY(toolbar.overflowButton(0) != toolbar.overflowButton(1));
    QVERIFY(toolbar.overflowPopup(0) != toolbar.overflowPopup(1));
    QCOMPARE(toolbar.minimumSizeHint().width(),
             toolbar.overflowButton(0)->width() + toolbar.overflowButton(1)->width() + 17);
    for (int i = 1; i < items.size(); ++i)
        QVERIFY(items.at(i - 1)->mapTo(&toolbar, QPoint()).x() <
                items.at(i)->mapTo(&toolbar, QPoint()).x());

    QSignalSpy relocations(&toolbar, &AdaptiveToolBar::widgetAboutToRelocate);
    bool signalledBeforeReparent = true;
    connect(&toolbar, &AdaptiveToolBar::widgetAboutToRelocate, &toolbar,
            [&](QWidget *widget, QWidget *destination) {
                signalledBeforeReparent &= widget->parentWidget() != destination;
            });
    const int narrow = qMax(toolbar.minimumWidth(), toolbar.minimumSizeHint().width());
    QList<int> masks(wide + 1);
    QList<int> thresholds;
    int previous = 0;
    for (int width = wide; width >= narrow; --width) {
        toolbar.resize(width, 36);
        settleLayout();
        QCOMPARE(toolbar.width(), width);
        const int mask = overflowMask(toolbar, items);
        masks[width] = mask;
        // No higher-priority item may disappear before a lower-priority one.
        for (int i = 1; i < items.size(); ++i) {
            if (mask & (1 << i))
                QVERIFY(mask & (1 << (i - 1)));
        }
        if (mask != previous)
            thresholds.append(width + 1);
        previous = mask;
        for (int i = 0; i < items.size(); ++i) {
            auto *item = items.at(i);
            if (mask & (1 << i)) {
                const int group = i < 2 ? 0 : 1;
                QVERIFY(toolbar.overflowPopup(group)->isAncestorOf(item));
                QVERIFY(!toolbar.overflowPopup(1 - group)->isAncestorOf(item));
                if (!popupHosts[i])
                    popupHosts[i] = item->parentWidget();
                QCOMPARE(item->parentWidget(), popupHosts.at(i).data());
            } else {
                QCOMPARE(item->parentWidget(), inlineHosts.at(i));
                QVERIFY(item->isVisible());
                QVERIFY(item->width() >= item->sizeHint().width());
            }
        }
        for (const int group : {0, 1}) {
            auto *more = toolbar.overflowButton(group);
            const int first = group * 2;
            const bool hasOverflow = (mask & (3 << first)) != 0;
            QCOMPARE(more->isVisible(), hasOverflow);
            QCOMPARE(more->parentWidget(), inlineHosts.at(first));
            if (!hasOverflow)
                continue;
            const QRect moreRect(more->mapTo(&toolbar, QPoint()), more->size());
            QVERIFY(toolbar.rect().contains(moreRect));
            QWidget *lastInline = nullptr;
            for (int i = first; i < first + 2; ++i) {
                if (!(mask & (1 << i)))
                    lastInline = items.at(i);
            }
            if (lastInline) {
                const QRect itemRect(lastInline->mapTo(&toolbar, QPoint()), lastInline->size());
                QCOMPARE(moreRect.left() - itemRect.right() - 1, 3);
            } else {
                QCOMPARE(more->parentWidget()->width(), more->width());
            }
        }
    }
    QCOMPARE(masks.at(narrow), 15);
    QVERIFY(!thresholds.isEmpty());
    QVERIFY(!relocations.isEmpty());
    QVERIFY(signalledBeforeReparent);
    for (const int group : {0, 1}) {
        QTest::mouseClick(toolbar.overflowButton(group), Qt::LeftButton);
        auto *popup = toolbar.overflowPopup(group);
        QTRY_VERIFY(popup->isVisible());
        QVERIFY(!toolbar.overflowPopup(1 - group)->isVisible());
        auto *first = items.at(group * 2);
        auto *second = items.at(group * 2 + 1);
        QVERIFY(first->isVisible() && second->isVisible());
        QCOMPARE(first->mapTo(popup, QPoint()).y(), second->mapTo(popup, QPoint()).y());
        QVERIFY(first->mapTo(popup, QPoint()).x() < second->mapTo(popup, QPoint()).x());
        toolbar.closeOverflow();
    }
    // Revisit both sides of every observed boundary, including the exact fit.
    for (const int threshold : thresholds) {
        for (const int width : {threshold - 1, threshold, threshold + 1}) {
            if (width < narrow || width > wide)
                continue;
            toolbar.resize(width, 36);
            settleLayout();
            QCOMPARE(overflowMask(toolbar, items), masks.at(width));
        }
    }
    // Growing must reverse the partition without hysteresis or replacement widgets.
    for (int width = narrow; width <= wide; ++width) {
        toolbar.resize(width, 36);
        settleLayout();
        QCOMPARE(overflowMask(toolbar, items), masks.at(width));
    }
    for (int i = 0; i < items.size(); ++i)
        QCOMPARE(items.at(i)->parentWidget(), inlineHosts.at(i));
    for (const int group : {0, 1})
        QVERIFY(!toolbar.overflowButton(group)->isVisible());
}

void GuiComponentTests::adaptiveToolbarInvalidatesHiddenSizesAndEmptyApplicability() {
    // Only the child toolbar is narrow or wider than a screen, never its native owner.
    QWidget root;
    root.resize(640, 120);
    AdaptiveToolBar toolbar(&root);
    toolbar.move(12, 12);
    auto *dynamic = new NaturalWidget(60);
    auto *important = new NaturalWidget(70);
    toolbar.addWidget(dynamic, {0, 0, 0});
    toolbar.addWidget(important, {1, 0, 10});
    toolbar.resize(500, 36);
    QVERIFY(showTestWindow(root));
    const int initialHint = toolbar.sizeHint().width();
    auto *inlineHost = dynamic->parentWidget();
    toolbar.resize(100, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(dynamic));
    QVERIFY(!dynamic->isVisible());
    toolbar.resize(initialHint - 1, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(dynamic));
    QVERIFY(!toolbar.overflowPopup()->isVisible());
    // A hidden child's updateGeometry must trigger the threshold change on its own.
    dynamic->setNaturalWidth(59);
    settleLayout();
    QCOMPARE(toolbar.sizeHint().width(), initialHint - 1);
    QVERIFY(!toolbar.isWidgetOverflowed(dynamic));
    QCOMPARE(dynamic->parentWidget(), inlineHost);
    dynamic->setNaturalWidth(60);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(dynamic));
    dynamic->setNaturalWidth(240);
    toolbar.invalidateItemSize(dynamic);
    settleLayout();
    QVERIFY(toolbar.sizeHint().width() >= initialHint + 180);
    toolbar.resize(initialHint + 30, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(dynamic));
    toolbar.resize(toolbar.sizeHint().width() + 20, 36);
    settleLayout();
    QVERIFY(!toolbar.isWidgetOverflowed(dynamic));
    QCOMPARE(dynamic->parentWidget(), inlineHost);
    QVERIFY(dynamic->width() >= 240);

    toolbar.setItemApplicable(dynamic, false);
    toolbar.setItemApplicable(important, false);
    settleLayout();
    QVERIFY(!toolbar.isItemApplicable(dynamic));
    QVERIFY(!toolbar.isItemApplicable(important));
    QVERIFY(!toolbar.isWidgetOverflowed(dynamic));
    QVERIFY(!toolbar.isWidgetOverflowed(important));
    QVERIFY(!dynamic->isVisible());
    QVERIFY(!important->isVisible());
    for (const int group : {0, 1}) {
        QVERIFY(!toolbar.overflowButton(group)->isVisible());
        QVERIFY(!toolbar.overflowPopup(group)->isVisible());
    }
    QCOMPARE(toolbar.minimumSizeHint().width(), 0);
    const int emptyHint = toolbar.sizeHint().width();
    toolbar.setItemApplicable(dynamic, true);
    toolbar.resize(500, 36);
    settleLayout();
    QVERIFY(dynamic->isVisible());
    QVERIFY(toolbar.sizeHint().width() > emptyHint);
    QVERIFY(!important->isVisible());
    QCOMPARE(dynamic->parentWidget(), inlineHost);
}

void GuiComponentTests::adaptiveToolbarHiddenHintsRearm_data() {
    QTest::addColumn<bool>("openPopupFirst");
    QTest::newRow("never-opened-popup") << false;
    QTest::newRow("previously-opened-popup") << true;
}

void GuiComponentTests::adaptiveToolbarHiddenHintsRearm() {
    QFETCH(bool, openPopupFirst);
    // Only the child toolbar is narrow or wider than a screen, never its native owner.
    QWidget root;
    root.resize(640, 120);
    AdaptiveToolBar toolbar(&root);
    toolbar.move(12, 12);
    toolbar.setItemSpacing(1);
    auto *first = new NaturalWidget(36);
    auto *middle = new NaturalWidget(36);
    auto *last = new NaturalWidget(36);
    toolbar.addWidget(first, {0, 0, 0});
    toolbar.addWidget(middle, {0, 1, 1});
    toolbar.addWidget(last, {0, 2, 2});
    toolbar.resize(81, 36);
    QVERIFY(showTestWindow(root));
    QCOMPARE(toolbar.width(), 81);
    QCOMPARE(toolbar.overflowButton()->width(), 24);
    QVERIFY(toolbar.isWidgetOverflowed(first));
    QVERIFY(toolbar.isWidgetOverflowed(middle));
    QVERIFY(!toolbar.isWidgetOverflowed(last));
    auto *inlineHost = last->parentWidget();
    auto *popupHost = middle->parentWidget();
    const QPointer<QWidget> identity(middle);
    auto *popup = toolbar.overflowPopup();
    QVERIFY(popup->isAncestorOf(first));
    QVERIFY(popup->isAncestorOf(middle));
    if (openPopupFirst) {
        QTest::mouseClick(toolbar.overflowButton(), Qt::LeftButton);
        QTRY_VERIFY(popup->isVisible());
        settleLayout();
        toolbar.closeOverflow();
        settleLayout();
    }
    QVERIFY(!popup->isVisible());
    // A no-op invalidation must not silence the following real hidden hint change.
    middle->updateGeometry();
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(middle));
    middle->setNaturalWidth(20);
    settleLayout();
    // Retaining middle would need 20 + 1 + 36 + 1 + More24 = 82 pixels.
    QVERIFY(toolbar.isWidgetOverflowed(middle));
    QCOMPARE(middle->parentWidget(), popupHost);
    QVERIFY(!popup->isVisible());
    // A second hidden updateGeometry must rearm layout delivery without manual invalidation.
    middle->setNaturalWidth(18);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(first));
    QVERIFY(!toolbar.isWidgetOverflowed(middle));
    QVERIFY(!toolbar.isWidgetOverflowed(last));
    QCOMPARE(identity.data(), middle);
    QCOMPARE(middle->parentWidget(), inlineHost);
    QCOMPARE(middle->size(), QSize(18, 28));
    QCOMPARE(last->size(), QSize(36, 28));
    QVERIFY(!popup->isVisible());
}

void GuiComponentTests::adaptiveToolbarClosingDoesNotStealNewEditorFocus() {
    QWidget root;
    root.resize(360, 140);
    AdaptiveToolBar toolbar(&root);
    toolbar.setGeometry(0, 0, 120, 36);
    QLineEdit editor(&root);
    editor.setGeometry(0, 70, 260, 28);
    auto *item = new NaturalWidget(320);
    item->setFocusPolicy(Qt::StrongFocus);
    toolbar.addWidget(item, {0, 0, 0});
    QVERIFY(showTestWindow(root));
    QTRY_VERIFY(root.isActiveWindow());
    QVERIFY(toolbar.isWidgetOverflowed(item));
    toolbar.overflowButton()->setFocus();
    QTRY_VERIFY(toolbar.overflowButton()->hasFocus());
    QTest::mouseClick(toolbar.overflowButton(), Qt::LeftButton);
    QTRY_VERIFY(toolbar.overflowPopup()->isVisible() && item->hasFocus());
    toolbar.closeOverflow();
    // Reactivate the real window before assigning focus, without dispatching queued events.
    root.activateWindow();
    // The queued restoration must respect a focus decision made after closing.
    editor.setFocus();
    settleLayout();
    QVERIFY(!toolbar.overflowPopup()->isVisible());
    QTRY_VERIFY(editor.hasFocus());
    settleLayout();
    QVERIFY(editor.hasFocus());
}

void GuiComponentTests::adaptiveToolbarGroupPopupsKeepControlsAndSwitchWithoutReplay() {
    // Native replay uses OS hit-testing, so other applications must not cover its real target.
    QWidget root;
    root.setWindowFlag(Qt::WindowStaysOnTopHint);
    root.resize(640, 120);
    AdaptiveToolBar toolbar(&root);
    toolbar.move(12, 12);
    toolbar.setItemSpacing(1);
    toolbar.setGroupSpacing(18);
    auto *name = new QLineEdit(QStringLiteral("Original name"));
    name->setFixedSize(128, 28);
    auto *singer = new NaturalWidget(100);
    auto *tool = new Button;
    tool->setFixedSize(36, 28);
    tool->setCheckable(true);
    tool->setChecked(true);
    auto *important = new NaturalWidget(40);
    toolbar.addWidget(name, {0, 0, 0});
    toolbar.addWidget(singer, {0, 1, 10});
    toolbar.addWidget(tool, {1, 0, 20});
    toolbar.addWidget(important, {1, 1, 30});
    toolbar.resize(600, 36);
    QVERIFY(showTestWindow(root));
    auto *nameHost = name->parentWidget();
    auto *toolHost = tool->parentWidget();
    auto *nameMore = toolbar.overflowButton(0);
    auto *toolMore = toolbar.overflowButton(1);
    auto *namePopup = toolbar.overflowPopup(0);
    auto *toolPopup = toolbar.overflowPopup(1);
    QCOMPARE(nameMore->parentWidget(), nameHost);
    QCOMPARE(toolMore->parentWidget(), toolHost);
    QVERIFY(qobject_cast<ControlGroup *>(nameHost));
    QVERIFY(qobject_cast<ControlGroup *>(toolHost));
    QVERIFY(nameHost != toolHost);
    QSignalSpy nameClicks(nameMore, &QAbstractButton::clicked);
    QSignalSpy toolClicks(toolMore, &QAbstractButton::clicked);
    QSignalSpy toggles(tool, &QAbstractButton::toggled);
    toolbar.resize(130, 36);
    settleLayout();
    QCOMPARE(toolbar.width(), 130);
    QVERIFY(toolbar.isWidgetOverflowed(name));
    QVERIFY(!toolbar.isWidgetOverflowed(tool));
    QTest::mouseClick(nameMore, Qt::LeftButton);
    QTRY_VERIFY(namePopup->isVisible() && name->isVisible());
    QVERIFY(QTest::qWaitForWindowExposed(namePopup));
    name->setFocus();
    QTRY_VERIFY(name->hasFocus());
    QSignalSpy editingFinished(name, &QLineEdit::editingFinished);
    QTest::keyClicks(name, QStringLiteral(" edited"));
    auto *stableNamePopupHost = name->parentWidget();
    toolbar.resize(110, 36);
    settleLayout();
    QCOMPARE(toolbar.width(), 110);
    // Migrating another group must preserve this open editor and its original host.
    QVERIFY(namePopup->isVisible());
    QVERIFY(name->hasFocus());
    QCOMPARE(name->parentWidget(), stableNamePopupHost);
    QCOMPARE(editingFinished.size(), 0);
    QVERIFY(toolbar.isWidgetOverflowed(name));
    QVERIFY(toolbar.isWidgetOverflowed(singer));
    QVERIFY(toolbar.isWidgetOverflowed(tool));
    QVERIFY(!toolbar.isWidgetOverflowed(important));
    QVERIFY(nameMore->isVisible() && toolMore->isVisible());
    QCOMPARE(nameMore->parentWidget(), nameHost);
    QCOMPARE(toolMore->parentWidget(), toolHost);
    QCOMPARE(nameHost->width(), nameMore->width());
    QCOMPARE(toolMore->x() - important->geometry().right() - 1, 1);
    QVERIFY(namePopup->isAncestorOf(name));
    QVERIFY(namePopup->isAncestorOf(singer));
    QVERIFY(!namePopup->isAncestorOf(tool));
    QVERIFY(!namePopup->isAncestorOf(important));
    QVERIFY(toolPopup->isAncestorOf(tool));
    QVERIFY(!toolPopup->isAncestorOf(name));
    const QPointer<QWidget> nameIdentity(name);
    const QPointer<QWidget> toolIdentity(tool);
    auto *namePopupHost = name->parentWidget();
    auto *toolPopupHost = tool->parentWidget();
    QVERIFY(!toolPopup->isVisible());
    QCOMPARE(nameClicks.size(), 1);
    QVERIFY(namePopup->windowHandle());
    const QPoint toolAnchor = toolMore->mapToGlobal(toolMore->rect().center());
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        // Offscreen cannot grab native input, so Qt cannot replay outside presses there.
        // Exercise the original anchor's switch slot here and real replay on Windows.
        toolMore->click();
    } else {
        QCOMPARE(QApplication::activePopupWidget(), namePopup);
        QCOMPARE(root.childAt(root.mapFromGlobal(toolAnchor)), toolMore);
        root.raise();
        namePopup->raise();
        QTRY_COMPARE(QApplication::widgetAt(toolAnchor), toolMore);
        QVERIFY(toolbar.window()->windowHandle());
        // Allow the replayed press to reach the owner before delivering its release. A QWindow
        // mouseClick would release into the now-hidden popup before processing the replay.
        QTest::mousePress(namePopup->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                          namePopup->mapFromGlobal(toolAnchor));
        QTRY_VERIFY(!namePopup->isVisible() && toolMore->isDown());
        QTest::mouseRelease(toolbar.window()->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                            toolbar.window()->mapFromGlobal(toolAnchor));
    }
    QTRY_VERIFY(!namePopup->isVisible() && toolPopup->isVisible());
    tool->setFocus();
    QTRY_VERIFY(tool->hasFocus());
    // A queued focus restore from the old popup must not reclaim the new focus.
    settleLayout();
    QVERIFY(tool->hasFocus());
    QCOMPARE(nameClicks.size(), 1);
    QCOMPARE(toolClicks.size(), 1);
    QVERIFY(tool->isVisible() && !name->isVisible());
    QCOMPARE(nameIdentity.data(), name);
    QCOMPARE(toolIdentity.data(), tool);
    QCOMPARE(name->parentWidget(), namePopupHost);
    QCOMPARE(tool->parentWidget(), toolPopupHost);
    QVERIFY(tool->isChecked());
    QCOMPARE(toggles.size(), 0);
    // Replaying onto the current anchor must close, not reopen, its popup.
    QVERIFY(toolPopup->windowHandle());
    QTest::mouseClick(toolPopup->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                      toolPopup->mapFromGlobal(toolAnchor));
    QTRY_VERIFY(!toolPopup->isVisible());
    settleLayout();
    QVERIFY(!namePopup->isVisible() && !toolPopup->isVisible());
    QCOMPARE(toolClicks.size(), 1);
    QTest::mouseClick(nameMore, Qt::LeftButton);
    QTRY_VERIFY(namePopup->isVisible());
    toolbar.closeOverflow();
    QVERIFY(!namePopup->isVisible() && !toolPopup->isVisible());
}

void GuiComponentTests::adaptiveToolbarRtlPopupTracksRuntimeDirection() {
    const auto applicationDirection = QApplication::layoutDirection();
    const auto restoreDirection = qScopeGuard(
        [applicationDirection] { QApplication::setLayoutDirection(applicationDirection); });
    QApplication::setLayoutDirection(Qt::LeftToRight);
    QWidget root;
    root.setLayoutDirection(Qt::LeftToRight);
    root.resize(600, 80);
    AdaptiveToolBar toolbar(&root);
    toolbar.setLayoutDirection(Qt::RightToLeft);
    toolbar.setItemSpacing(1);
    toolbar.setGroupSpacing(18);
    toolbar.setGeometry(0, 0, 500, 36);
    auto *first = new NaturalWidget(100);
    first->setFocusPolicy(Qt::StrongFocus);
    auto *second = new NaturalWidget(200);
    auto *important = new NaturalWidget(80);
    toolbar.addWidget(first, {0, 0, 0});
    toolbar.addWidget(second, {0, 1, 1});
    toolbar.addWidget(important, {1, 0, 2});
    const QPointer<QWidget> firstIdentity(first);
    const QPointer<QWidget> secondIdentity(second);
    QVERIFY(showTestWindow(root));
    auto *inlineHost = first->parentWidget();
    QCOMPARE(second->parentWidget(), inlineHost);
    auto *more = toolbar.overflowButton(0);
    auto *popup = toolbar.overflowPopup(0);
    QCOMPARE(more->parentWidget(), inlineHost);
    toolbar.resize(335, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(first));
    QVERIFY(!toolbar.isWidgetOverflowed(second));
    QVERIFY(!toolbar.isWidgetOverflowed(important));
    QVERIFY(more->isVisible());
    QCOMPARE(second->parentWidget(), inlineHost);
    // In RTL the group's trailing More is on the left of its retained originals.
    QCOMPARE(second->geometry().left() - more->geometry().right() - 1, 1);
    auto *popupHost = first->parentWidget();
    QVERIFY(popup->isAncestorOf(popupHost));
    toolbar.resize(250, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(first));
    QVERIFY(toolbar.isWidgetOverflowed(second));
    QVERIFY(!toolbar.isWidgetOverflowed(important));
    QCOMPARE(first->parentWidget(), popupHost);
    QCOMPARE(second->parentWidget(), popupHost);
    QTest::mouseClick(more, Qt::LeftButton);
    QTRY_VERIFY(popup->isVisible() && first->isVisible() && second->isVisible());
    settleLayout();
    QCOMPARE(QApplication::layoutDirection(), Qt::LeftToRight);
    QCOMPARE(root.layoutDirection(), Qt::LeftToRight);
    QCOMPARE(popup->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(first->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(second->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(first->size(), QSize(100, 28));
    QCOMPARE(second->size(), QSize(200, 28));
    QCOMPARE(first->mapTo(popup, QPoint()).y(), second->mapTo(popup, QPoint()).y());
    QVERIFY(first->mapTo(popup, QPoint()).x() > second->mapTo(popup, QPoint()).x());
    first->setFocus();
    QTRY_VERIFY(first->hasFocus());
    toolbar.setLayoutDirection(Qt::LeftToRight);
    settleLayout();
    QVERIFY(popup->isVisible());
    QVERIFY(first->hasFocus());
    QCOMPARE(QApplication::layoutDirection(), Qt::LeftToRight);
    QCOMPARE(root.layoutDirection(), Qt::LeftToRight);
    QCOMPARE(popup->layoutDirection(), Qt::LeftToRight);
    QCOMPARE(first->layoutDirection(), Qt::LeftToRight);
    QCOMPARE(second->layoutDirection(), Qt::LeftToRight);
    QCOMPARE(firstIdentity.data(), first);
    QCOMPARE(secondIdentity.data(), second);
    QCOMPARE(first->parentWidget(), popupHost);
    QCOMPARE(second->parentWidget(), popupHost);
    QCOMPARE(first->size(), QSize(100, 28));
    QCOMPARE(second->size(), QSize(200, 28));
    QCOMPARE(first->mapTo(popup, QPoint()).y(), second->mapTo(popup, QPoint()).y());
    QVERIFY(first->mapTo(popup, QPoint()).x() < second->mapTo(popup, QPoint()).x());
    toolbar.closeOverflow();
    toolbar.resize(335, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(first));
    QVERIFY(!toolbar.isWidgetOverflowed(second));
    QCOMPARE(second->parentWidget(), inlineHost);
    QCOMPARE(more->parentWidget(), inlineHost);
    QCOMPARE(more->geometry().left() - second->geometry().right() - 1, 1);
}

void GuiComponentTests::adaptiveToolbarOversizedPopupWrapsWithoutSqueezing() {
    // Only the child toolbar is narrow or wider than a screen, never its native owner.
    QWidget root;
    root.resize(640, 120);
    AdaptiveToolBar toolbar(&root);
    toolbar.move(12, 12);
    toolbar.resize(160, 36);
    QVERIFY(showTestWindow(root));
    auto *screen = toolbar.screen();
    QVERIFY(screen);
    const QRect available = screen->availableGeometry();
    QVERIFY(available.isValid());
    const int count = qMax(50, available.width() / 37 + 5);
    QList<QPointer<QWidget>> identities;
    for (int i = 0; i < count; ++i) {
        auto *item = new NaturalWidget(36);
        item->setObjectName(QStringLiteral("wrappingItem%1").arg(i));
        identities.append(item);
        toolbar.addWidget(item, {0, i, i});
    }
    settleLayout();
    QVERIFY(toolbar.overflowButton()->isVisible());
    QTest::mouseClick(toolbar.overflowButton(), Qt::LeftButton);
    auto *popup = toolbar.overflowPopup();
    QTRY_VERIFY(popup->isVisible());
    settleLayout();
    QVERIFY(popup->findChildren<QAbstractScrollArea *>().isEmpty());
    QVERIFY(popup->findChildren<QScrollBar *>().isEmpty());
    QVERIFY(available.contains(QRect(popup->mapToGlobal(QPoint()), popup->size())));
    const auto frameMargins = popup->contentsMargins();
    QList<int> rowTops;
    QRect previous;
    for (int i = 0; i < identities.size(); ++i) {
        auto *item = identities.at(i).data();
        QVERIFY(item);
        QCOMPARE(item->size(), QSize(36, 28));
        if (!toolbar.isWidgetOverflowed(item)) {
            QVERIFY(!popup->isAncestorOf(item));
            continue;
        }
        QCOMPARE(popup->findChild<QWidget *>(QStringLiteral("wrappingItem%1").arg(i)), item);
        QVERIFY(item->isVisible());
        const QRect itemRect(item->mapTo(popup, QPoint()), item->size());
        QVERIFY(popup->rect().contains(itemRect));
        if (rowTops.isEmpty() || itemRect.top() != rowTops.last()) {
            if (!rowTops.isEmpty())
                QCOMPARE(itemRect.top() - previous.bottom() - 1, 4);
            else
                QCOMPARE(itemRect.top(), frameMargins.top() + 4);
            QCOMPARE(itemRect.left(), frameMargins.left() + 4);
            rowTops.append(itemRect.top());
        } else {
            QCOMPARE(itemRect.left() - previous.right() - 1, 1);
        }
        previous = itemRect;
    }
    QVERIFY(rowTops.size() > 1);
    QCOMPARE(popup->height(), rowTops.size() * 28 + (rowTops.size() - 1) * 4 + 8 +
                                  frameMargins.top() + frameMargins.bottom());
    QCOMPARE(popup->height() - previous.bottom() - 1, frameMargins.bottom() + 4);
    toolbar.closeOverflow();
    const int wide = toolbar.sizeHint().width();
    toolbar.resize(wide, 36);
    settleLayout();
    QCOMPARE(toolbar.width(), wide);
    for (const auto &identity : identities) {
        QVERIFY(identity);
        QVERIFY(!toolbar.isWidgetOverflowed(identity));
        QCOMPARE(identity->size(), QSize(36, 28));
    }
}

void GuiComponentTests::adaptiveToolbarTeardownCancelsPendingLayout() {
    auto toolbar = std::make_unique<AdaptiveToolBar>();
    auto *item = new NaturalWidget(320);
    toolbar->addWidget(item, {0, 0, 0});
    toolbar->resize(120, 36);
    toolbar->show();
    toolbar->activateWindow();
    settleLayout();
    QVERIFY(toolbar->isWidgetOverflowed(item));
    QTest::mouseClick(toolbar->overflowButton(), Qt::LeftButton);
    QPointer<QWidget> popup(toolbar->overflowPopup());
    QPointer<QWidget> registeredItem(item);
    QTRY_VERIFY(popup->isVisible());
    delete item;
    QVERIFY(!registeredItem);
    toolbar->invalidateItemSize();
    // Do not dispatch the queued layout before destroying its owner.
    toolbar.reset();
    QVERIFY(!popup);
    settleLayout();
    QVERIFY(!registeredItem);
}

void GuiComponentTests::adaptiveToolbarPopupUsesTheOriginalCombo() {
    // Only the child toolbar is narrow or wider than a screen, never its native owner.
    QWidget root;
    root.resize(640, 120);
    AdaptiveToolBar toolbar(&root);
    toolbar.move(12, 12);
    auto *combo = new ComboBox;
    combo->addItems({QStringLiteral("First"), QStringLiteral("Second"), QStringLiteral("Third")});
    combo->setMinimumWidth(150);
    auto *important = new NaturalWidget(80);
    toolbar.addWidget(combo, {0, 0, 0});
    toolbar.addWidget(important, {1, 0, 10});
    toolbar.resize(500, 36);
    QVERIFY(showTestWindow(root));
    auto *inlineHost = combo->parentWidget();
    const QPointer<ComboBox> identity(combo);
    QSignalSpy changed(combo, &QComboBox::currentIndexChanged);
    toolbar.resize(100, 36);
    settleLayout();
    QVERIFY(toolbar.isWidgetOverflowed(combo));
    QTest::mouseClick(toolbar.overflowButton(), Qt::LeftButton);
    QTRY_VERIFY(toolbar.overflowPopup()->isVisible() && combo->isVisible());
    auto *popupHost = combo->parentWidget();
    QTest::mouseClick(combo, Qt::LeftButton);
    auto *view = combo->view();
    QTRY_VERIFY(view->isVisible());
    const auto index = combo->model()->index(1, combo->modelColumn(), combo->rootModelIndex());
    // Qt blocks immediate mouse release after opening for its double-click interval.
    // Keyboard navigation exercises the original popup without depending on that timer.
    QTest::keyClick(view, Qt::Key_Down);
    QCOMPARE(view->currentIndex(), index);
    QTest::keyClick(view, Qt::Key_Return);
    QTRY_COMPARE(combo->currentIndex(), 1);
    QCOMPARE(changed.size(), 1);
    QCOMPARE(identity.data(), combo);
    QTest::mouseClick(combo, Qt::LeftButton);
    QTRY_VERIFY(view->isVisible());
    toolbar.resize(500, 36);
    settleLayout();
    QTRY_VERIFY(!view->isVisible());
    QVERIFY(!toolbar.overflowPopup()->isVisible());
    QCOMPARE(changed.size(), 1);
    QCOMPARE(combo->parentWidget(), inlineHost);
    QCOMPARE(combo->currentIndex(), 1);
    QCOMPARE(identity.data(), combo);
    toolbar.resize(100, 36);
    settleLayout();
    QCOMPARE(combo->parentWidget(), popupHost);
    QTest::mouseClick(toolbar.overflowButton(), Qt::LeftButton);
    QTRY_VERIFY(toolbar.overflowPopup()->isVisible());
    QTest::keyClick(toolbar.overflowPopup(), Qt::Key_Escape);
    QTRY_VERIFY(!toolbar.overflowPopup()->isVisible());
    QTest::mouseClick(toolbar.overflowButton(), Qt::LeftButton);
    auto *popup = toolbar.overflowPopup();
    QTRY_VERIFY(popup->isVisible());
    QVERIFY(popup->windowHandle());
    const QPoint anchor =
        toolbar.overflowButton()->mapToGlobal(toolbar.overflowButton()->rect().center());
    // Use the native-window route so Qt's outside-popup mouse replay participates.
    QTest::mouseClick(popup->windowHandle(), Qt::LeftButton, Qt::NoModifier,
                      popup->mapFromGlobal(anchor));
    settleLayout();
    QVERIFY(!popup->isVisible());
    settleLayout();
    QVERIFY(!popup->isVisible());
}
