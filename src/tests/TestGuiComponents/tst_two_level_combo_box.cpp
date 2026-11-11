#include "tst_gui_components.h"

#include "UI/Controls/TwoLevelComboBox.h"

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Theme/ThemeLoader.h>

#include <QtTest/QTest>
#include <QVersionNumber>
#include <QAction>
#include <QColor>
#include <QDir>
#include <QHash>
#include <QImage>
#include <QKeySequence>
#include <QMenu>
#include <QPixmap>
#include <QWidget>

namespace {
    // Matches QMenu::right-arrow in controls.qss.
    constexpr int kArrowWidth = 14;
    constexpr int kArrowRightInset = 6;
    constexpr int kRequiredGap = 12;

    QRgb dominantColor(const QImage &image, const QRect &area) {
        QHash<QRgb, int> histogram;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                histogram[image.pixelColor(x, y).rgb()]++;
        QRgb best = 0;
        int bestCount = -1;
        for (auto it = histogram.cbegin(); it != histogram.cend(); ++it) {
            if (it.value() > bestCount) {
                bestCount = it.value();
                best = it.key();
            }
        }
        return best;
    }

    bool isInk(QRgb pixel, QRgb background) {
        return qAbs(qRed(pixel) - qRed(background)) + qAbs(qGreen(pixel) - qGreen(background)) +
                   qAbs(qBlue(pixel) - qBlue(background)) >
               90;
    }

    bool columnHasInk(const QImage &image, int x, int top, int bottom, QRgb background) {
        for (int y = top; y <= bottom; ++y) {
            if (isInk(image.pixelColor(x, y).rgb(), background))
                return true;
        }
        return false;
    }

    int rightmostInkColumn(const QImage &image, int from, int to, int top, int bottom,
                           QRgb background) {
        for (int x = to; x >= from; --x) {
            if (columnHasInk(image, x, top, bottom, background))
                return x;
        }
        return -1;
    }

    int colorDistance(const QColor &a, const QColor &b) {
        return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) +
               qAbs(a.blue() - b.blue());
    }

    // The purest text pixel in an area. Antialiasing blends the text colour into
    // the background, so the pixel furthest from the background is the best
    // approximation of the pen actually used.
    QColor strongestInk(const QImage &image, const QRect &area, QRgb background) {
        const QColor bg(background);
        int bestDistance = -1;
        QRgb bestPixel = background;
        for (int y = area.top(); y <= area.bottom(); ++y) {
            for (int x = area.left(); x <= area.right(); ++x) {
                const QRgb pixel = image.pixelColor(x, y).rgb();
                const int distance = colorDistance(QColor::fromRgb(pixel), bg);
                if (distance > bestDistance) {
                    bestDistance = distance;
                    bestPixel = pixel;
                }
            }
        }
        return QColor::fromRgb(bestPixel);
    }

    // Distance from the item's right edge to the last ink pixel of its shortcut
    // column. QStyleSheetStyle right-aligns shortcut text to contentsRect().right(),
    // so this offset grows by the whole extra padding when the arrow reservation
    // is wrongly applied to a menu that has shortcuts.
    int shortcutRightOffset(QMenu *menu, QAction *action) {
        QPixmap pixmap(menu->size());
        pixmap.fill(Qt::transparent);
        menu->render(&pixmap);
        const QImage image = pixmap.toImage();
        const QRect item = menu->actionGeometry(action);
        const QRgb background = dominantColor(image, item.adjusted(2, 2, -2, -2));
        const int last = rightmostInkColumn(image, item.left(), item.right(), item.top() + 2,
                                            item.bottom() - 2, background);
        return item.right() - last;
    }
}

void GuiComponentTests::mixDisplay_data() {
    QTest::addColumn<bool>("inherited");
    QTest::newRow("follow-track") << true;
    QTest::newRow("explicit-mix") << false;
}

void GuiComponentTests::mixDisplay() {
    QFETCH(bool, inherited);
    TwoLevelComboBox comboBox;
    comboBox.setShowInheritItem(true);
    comboBox.setItems({});
    const SpeakerInfo speaker(QStringLiteral("speaker"), QStringLiteral("Speaker"));
    const SingerInfo singer(
        {QStringLiteral("singer"), QStringLiteral("package"), QVersionNumber(1, 0)},
        QStringLiteral("Singer"), {speaker});
    comboBox.addItem(QStringLiteral("Speaker"), singer, speaker);
    comboBox.setCurrentData(singer, speaker, inherited);
    comboBox.setDisplayTextOverride(QStringLiteral("Singer / Mix"));
    QCOMPARE(comboBox.isInheritSelected(), inherited);
    QCOMPARE(comboBox.currentText(), inherited ? QStringLiteral("Follow Track (Singer / Mix)")
                                               : QStringLiteral("Singer / Mix"));
    comboBox.setCurrentData(singer, speaker, !inherited);
    QCOMPARE(comboBox.isInheritSelected(), !inherited);
}

void GuiComponentTests::selectSinger_data() {
    QTest::addColumn<bool>("multipleSpeakers");
    QTest::newRow("single-speaker") << false;
    QTest::newRow("speaker-submenu") << true;
}

void GuiComponentTests::selectSinger() {
    QFETCH(bool, multipleSpeakers);
    const SpeakerInfo speaker(QStringLiteral("internal_emb"),
                              QStringLiteral("Configured Emb Name"));
    QList<SpeakerInfo> speakers{speaker};
    if (multipleSpeakers)
        speakers.append(SpeakerInfo(QStringLiteral("second_emb"), QStringLiteral("Second")));
    const SingerInfo singer(
        {QStringLiteral("singer-id"), QStringLiteral("package-id"), QVersionNumber(1, 0)},
        QStringLiteral("Configured Role Name"), speakers);
    const PackageInfo package(QStringLiteral("package-id"), QVersionNumber(1, 0), {}, {}, {}, {},
                              {}, {}, {singer});
    TwoLevelComboBox comboBox;
    comboBox.setItems({package});
    comboBox.show();

    auto *menu = comboBox.mainMenu();
    QAction *singerAction = nullptr;
    for (auto *action : menu->actions()) {
        if (action->text() == singer.name())
            singerAction = action;
    }
    QVERIFY(singerAction);
    QCOMPARE(singerAction->menu() != nullptr, multipleSpeakers);
    QMenu *selectionMenu = menu;
    QAction *selectionAction = singerAction;
    if (multipleSpeakers) {
        selectionMenu = singerAction->menu();
        selectionAction = nullptr;
        for (auto *action : selectionMenu->actions()) {
            if (action->text() == speaker.name())
                selectionAction = action;
        }
    }
    QVERIFY(selectionAction);
    selectionMenu->popup(comboBox.mapToGlobal(QPoint(0, comboBox.height())));
    QTRY_VERIFY(selectionMenu->isVisible());
    QTest::mouseClick(selectionMenu, Qt::LeftButton, Qt::NoModifier,
                      selectionMenu->actionGeometry(selectionAction).center());
    QTRY_VERIFY(comboBox.currentSinger() == singer);
    QVERIFY(comboBox.currentSpeaker() == speaker);
}

void GuiComponentTests::submenuLabelKeepsGapBeforeArrow_data() {
    QTest::addColumn<QString>("themeId");
    QTest::addColumn<bool>("checkableItems");
    QTest::newRow("dark-checkable") << QStringLiteral("lite-dark") << true;
    QTest::newRow("dark-plain") << QStringLiteral("lite-dark") << false;
    QTest::newRow("light-checkable") << QStringLiteral("lite-light") << true;
    QTest::newRow("light-plain") << QStringLiteral("lite-light") << false;
}

void GuiComponentTests::submenuLabelKeepsGapBeforeArrow() {
    QFETCH(QString, themeId);
    QFETCH(bool, checkableItems);

    const auto theme = ThemeLoader::load(themeId);
    QVERIFY2(theme.has_value(), qPrintable(ThemeLoader::lastError()));

    QWidget root;
    root.setStyleSheet(theme->styleSheet);

    TwoLevelComboBox comboBox(&root);
    const QString longLabel = QStringLiteral("Shisakune-Demo-Long-Singer-Name-0811");
    comboBox.addGroup(longLabel);
    if (checkableItems) {
        auto *checkable = comboBox.mainMenu()->addAction(QStringLiteral("(No singer)"));
        checkable->setCheckable(true);
        checkable->setChecked(true);
    }

    auto *menu = comboBox.mainMenu();
    QAction *submenuAction = nullptr;
    for (auto *action : menu->actions()) {
        if (action->text() == longLabel) {
            submenuAction = action;
            break;
        }
    }
    QVERIFY(submenuAction);
    QVERIFY(submenuAction->menu());
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), true);

    root.show();
    QApplication::processEvents();
    menu->popup(root.mapToGlobal(QPoint(10, 10)));
    QTRY_VERIFY(menu->isVisible());
    QApplication::processEvents();

    const QRect item = menu->actionGeometry(submenuAction);
    QVERIFY2(item.width() > kArrowWidth + kArrowRightInset + kRequiredGap,
             qPrintable(QStringLiteral("item is unexpectedly narrow: %1").arg(item.width())));

    QPixmap pixmap(menu->size());
    pixmap.fill(Qt::transparent);
    menu->render(&pixmap);
    const QImage image = pixmap.toImage();
    QCOMPARE(image.size(), menu->size());

    // Optional visual evidence, off by default.
    const QString screenshotDir = qEnvironmentVariable("LITE_MENU_SCREENSHOT_DIR");
    if (!screenshotDir.isEmpty()) {
        const QDir output(screenshotDir);
        if (output.mkpath(QStringLiteral("."))) {
            pixmap.save(output.filePath(
                QStringLiteral("submenu-gap-%1-%2.png")
                    .arg(themeId, checkableItems ? QStringLiteral("checkable")
                                                 : QStringLiteral("plain"))));
        }
    }

    // The custom chevron in Menu::paintEvent starts at item.right() - 6 - 14.
    const int arrowLeft = item.right() - kArrowRightInset - kArrowWidth;
    const int top = item.top() + 2;
    const int bottom = item.bottom() - 2;
    const QRgb background = dominantColor(image, item.adjusted(2, 2, -2, -2));

    const int lastTextColumn =
        rightmostInkColumn(image, item.left(), arrowLeft - 1, top, bottom, background);
    QVERIFY2(lastTextColumn > item.left(), "the long label was not rendered");

    const int gap = arrowLeft - lastTextColumn - 1;
    QVERIFY2(gap >= kRequiredGap,
             qPrintable(QStringLiteral("%1/%2: text ends only %3px before the arrow (need %4)")
                            .arg(themeId, checkableItems ? QStringLiteral("checkable")
                                                         : QStringLiteral("plain"))
                            .arg(gap)
                            .arg(kRequiredGap)));

    menu->hide();
}

void GuiComponentTests::submenuArrowSpaceTracksActionChanges() {
    const auto theme = ThemeLoader::load(QStringLiteral("lite-dark"));
    QVERIFY2(theme.has_value(), qPrintable(ThemeLoader::lastError()));

    QWidget root;
    root.setStyleSheet(theme->styleSheet);
    TwoLevelComboBox comboBox(&root);
    auto *menu = comboBox.mainMenu();

    auto *action = menu->addAction(QStringLiteral("Shisakune-Demo-Long-Singer-Name-0811"));
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), false);

    root.show();
    QApplication::processEvents();
    menu->popup(root.mapToGlobal(QPoint(10, 10)));
    QTRY_VERIFY(menu->isVisible());
    QApplication::processEvents();

    // The menu is already polished here. Adding a submenu must re-evaluate the
    // attribute selector, otherwise repopulating TwoLevelComboBox after the
    // popup has been shown once would drop the reservation.
    const int plainWidth = menu->sizeHint().width();
    action->setMenu(new QMenu(menu));
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), true);
    const int subMenuWidth = menu->sizeHint().width();
    QVERIFY2(subMenuWidth - plainWidth >= 30,
             qPrintable(QStringLiteral("stylesheet was not re-evaluated: %1 -> %2")
                            .arg(plainWidth)
                            .arg(subMenuWidth)));

    // Introducing a shortcut column hands the right edge over to the shortcuts,
    // so the arrow reservation must be dropped even though the submenu is still
    // there. Otherwise every shortcut would be pushed left by the padding.
    auto *shortcutAction = menu->addAction(QStringLiteral("Save"));
    shortcutAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    shortcutAction->setShortcutVisibleInContextMenu(true);
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), false);

    menu->hide();
    menu->clear();
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), false);
}

void GuiComponentTests::submenuArrowSpaceKeepsShortcutColumnAligned_data() {
    QTest::addColumn<QString>("themeId");
    QTest::newRow("dark") << QStringLiteral("lite-dark");
    QTest::newRow("light") << QStringLiteral("lite-light");
}

void GuiComponentTests::submenuArrowSpaceKeepsShortcutColumnAligned() {
    QFETCH(QString, themeId);
    const auto theme = ThemeLoader::load(themeId);
    QVERIFY2(theme.has_value(), qPrintable(ThemeLoader::lastError()));

    QWidget root;
    root.setStyleSheet(theme->styleSheet);
    TwoLevelComboBox comboBox(&root);
    auto *menu = comboBox.mainMenu();

    auto *shortcutAction = menu->addAction(QStringLiteral("Save"));
    shortcutAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    shortcutAction->setShortcutVisibleInContextMenu(true);
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), false);

    root.show();
    QApplication::processEvents();
    menu->popup(root.mapToGlobal(QPoint(10, 10)));
    QTRY_VERIFY(menu->isVisible());
    QApplication::processEvents();

    const int offsetWithoutSubMenu = shortcutRightOffset(menu, shortcutAction);

    // Adding a submenu used to switch on the arrow reservation and shift the
    // shortcut column 30px to the left (see the File menu regression).
    auto *subAction = menu->addAction(QStringLiteral("More"));
    subAction->setMenu(new QMenu(menu));
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), false);
    QApplication::processEvents();

    const int offsetWithSubMenu = shortcutRightOffset(menu, shortcutAction);
    QVERIFY2(qAbs(offsetWithSubMenu - offsetWithoutSubMenu) <= 2,
             qPrintable(QStringLiteral("%1: shortcut column moved %2 -> %3 px from the right edge")
                            .arg(themeId)
                            .arg(offsetWithoutSubMenu)
                            .arg(offsetWithSubMenu)));
    QVERIFY2(offsetWithSubMenu <= 12,
             qPrintable(QStringLiteral("%1: shortcut column sits %2 px from the right edge")
                            .arg(themeId)
                            .arg(offsetWithSubMenu)));

    menu->hide();
}

void GuiComponentTests::shortcutTextUsesIndependentThemeColors_data() {
    QTest::addColumn<QString>("themeId");
    QTest::newRow("dark") << QStringLiteral("lite-dark");
    QTest::newRow("light") << QStringLiteral("lite-light");
}

void GuiComponentTests::shortcutTextUsesIndependentThemeColors() {
    QFETCH(QString, themeId);
    const auto theme = ThemeLoader::load(themeId);
    QVERIFY2(theme.has_value(), qPrintable(ThemeLoader::lastError()));

    const QColor primary = theme->semanticColors.value(QStringLiteral("text.primary"));
    const QColor secondary = theme->semanticColors.value(QStringLiteral("text.secondary"));
    QVERIFY(primary.isValid());
    QVERIFY(secondary.isValid());
    QVERIFY2(colorDistance(primary, secondary) > 12, "theme tokens are too close to tell apart");

    QWidget root;
    root.setStyleSheet(theme->styleSheet);
    TwoLevelComboBox comboBox(&root);
    auto *menu = comboBox.mainMenu();

    auto *labelAction = menu->addAction(QStringLiteral("Save"));
    auto *shortcutAction = menu->addAction(QStringLiteral("Open"));
    shortcutAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    shortcutAction->setShortcutVisibleInContextMenu(true);

    root.show();
    QApplication::processEvents();
    menu->popup(root.mapToGlobal(QPoint(10, 10)));
    QTRY_VERIFY(menu->isVisible());
    QApplication::processEvents();

    // Qt has no ::shortcut subcontrol, so the colours arrive as Menu qproperties.
    // Theme tokens are floats but QSS serialises them as 8-bit hex, so allow the
    // one-step-per-channel quantisation instead of comparing exactly.
    QVERIFY2(colorDistance(menu->shortcutTextColor(), secondary) <= 3,
             qPrintable(QStringLiteral("%1: shortcutTextColor %2 != %3")
                            .arg(themeId, menu->shortcutTextColor().name(), secondary.name())));
    QVERIFY2(colorDistance(menu->shortcutSelectedTextColor(), primary) <= 3,
             qPrintable(QStringLiteral("%1: shortcutSelectedTextColor %2 != %3")
                            .arg(themeId, menu->shortcutSelectedTextColor().name(),
                                 primary.name())));

    auto render = [&menu] {
        QPixmap pixmap(menu->size());
        pixmap.fill(Qt::transparent);
        menu->render(&pixmap);
        return pixmap.toImage();
    };

    const QRect labelItem = menu->actionGeometry(labelAction);
    const QRect shortcutItem = menu->actionGeometry(shortcutAction);
    const QRect labelArea(labelItem.left() + 12, labelItem.top() + 2, 110,
                          labelItem.height() - 4);
    const QRect shortcutArea(shortcutItem.right() - 100, shortcutItem.top() + 2, 96,
                             shortcutItem.height() - 4);

    QImage image = render();
    const QRgb labelBg = dominantColor(image, labelItem.adjusted(2, 2, -2, -2));
    const QRgb shortcutBg = dominantColor(image, shortcutItem.adjusted(2, 2, -2, -2));
    const QColor labelInk = strongestInk(image, labelArea, labelBg);
    const QColor shortcutInk = strongestInk(image, shortcutArea, shortcutBg);

    QVERIFY2(colorDistance(labelInk, primary) < colorDistance(labelInk, secondary),
             qPrintable(QStringLiteral("%1: label ink %2 is not text.primary %3")
                            .arg(themeId, labelInk.name(), primary.name())));
    QVERIFY2(colorDistance(shortcutInk, secondary) < colorDistance(shortcutInk, primary),
             qPrintable(QStringLiteral("%1: shortcut ink %2 is not text.secondary %3")
                            .arg(themeId, shortcutInk.name(), secondary.name())));

    // A selected row brings the shortcut back to full strength.
    menu->setActiveAction(shortcutAction);
    menu->update();
    QApplication::processEvents();
    image = render();
    const QRgb selectedBg = dominantColor(image, shortcutItem.adjusted(2, 2, -2, -2));
    const QColor selectedInk = strongestInk(image, shortcutArea, selectedBg);
    QVERIFY2(colorDistance(selectedInk, primary) < colorDistance(selectedInk, secondary),
             qPrintable(QStringLiteral("%1: selected shortcut ink %2 is not text.primary %3")
                            .arg(themeId, selectedInk.name(), primary.name())));

    menu->hide();
}

void GuiComponentTests::submenuShortcutKeepsThemeColorAndGap_data() {
    QTest::addColumn<QString>("themeId");
    QTest::newRow("dark") << QStringLiteral("lite-dark");
    QTest::newRow("light") << QStringLiteral("lite-light");
}

void GuiComponentTests::submenuShortcutKeepsThemeColorAndGap() {
    QFETCH(QString, themeId);
    const auto theme = ThemeLoader::load(themeId);
    QVERIFY2(theme.has_value(), qPrintable(ThemeLoader::lastError()));

    const QColor primary = theme->semanticColors.value(QStringLiteral("text.primary"));
    const QColor secondary = theme->semanticColors.value(QStringLiteral("text.secondary"));
    QVERIFY(primary.isValid());
    QVERIFY(secondary.isValid());

    // Earlier menu cases may still own the popup grab under offscreen; release
    // it before asking QStyleSheetStyle to polish a fresh menu.
    if (auto *popup = QApplication::activePopupWidget())
        popup->hide();
    QApplication::processEvents();

    QWidget root;
    root.setStyleSheet(theme->styleSheet);
    TwoLevelComboBox comboBox(&root);
    auto *menu = comboBox.mainMenu();

    menu->addAction(QStringLiteral("Copy"));
    auto *plainShortcut = menu->addAction(QStringLiteral("Save"));
    plainShortcut->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    plainShortcut->setShortcutVisibleInContextMenu(true);
    auto *subAction = menu->addAction(QStringLiteral("Recent"));
    subAction->setMenu(new QMenu(menu));
    subAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    subAction->setShortcutVisibleInContextMenu(true);
    QCOMPARE(menu->property("submenuArrowSpace").toBool(), false);

    root.show();
    QApplication::processEvents();
    menu->popup(root.mapToGlobal(QPoint(10, 10)));
    QTRY_VERIFY(menu->isVisible());
    QApplication::processEvents();
    QVERIFY2(menu->shortcutTextColor().isValid(),
             qPrintable(QStringLiteral("%1: theme qproperty was not applied").arg(themeId)));

    QPixmap pixmap(menu->size());
    pixmap.fill(Qt::transparent);
    menu->render(&pixmap);
    const QImage image = pixmap.toImage();
    const QRect item = menu->actionGeometry(subAction);
    // Offscreen clamps a popup to the virtual screen; never sample pixels past
    // the rendered pixmap, otherwise QImage returns black and the ink test
    // measures an imaginary chevron.
    QVERIFY2(item.right() < image.width(),
             qPrintable(QStringLiteral("%1: popup clipped to %2 but item ends at %3")
                            .arg(themeId)
                            .arg(image.width())
                            .arg(item.right())));
    const QRgb background = dominantColor(image, item.adjusted(2, 2, -2, -2));
    const int chevronLeft = item.right() - kArrowRightInset - kArrowWidth;
    const int top = item.top() + 2;
    const int bottom = item.bottom() - 2;

    // Codex finding: clearSlot(arrowSlot) used to erase the right-hand part of
    // a submenu row's shortcut while that row skipped the themed repaint, so
    // the remaining text was left in the label's primary colour.
    const int lastShortcutInk =
        rightmostInkColumn(image, item.left(), chevronLeft - 1, top, bottom, background);
    QVERIFY2(lastShortcutInk > item.left(),
             qPrintable(QStringLiteral("%1: shortcut text was erased").arg(themeId)));
    const QRect inkArea(qMax(item.left(), lastShortcutInk - 40), top, 41,
                        bottom - top + 1);
    const QColor shortcutInk = strongestInk(image, inkArea, background);
    QVERIFY2(colorDistance(shortcutInk, secondary) < colorDistance(shortcutInk, primary),
             qPrintable(QStringLiteral("%1: ink %2 != %3 | menu %4x%5 item %6..%7 last %8 chevron %9 bg %10")
                            .arg(themeId, shortcutInk.name(), secondary.name())
                            .arg(menu->width())
                            .arg(menu->height())
                            .arg(item.left())
                            .arg(item.right())
                            .arg(lastShortcutInk)
                            .arg(chevronLeft)
                            .arg(QColor::fromRgb(background).name())));

    const int gap = chevronLeft - lastShortcutInk - 1;
    QVERIFY2(gap >= kRequiredGap,
             qPrintable(QStringLiteral("%1: shortcut ends only %2px before the chevron (need %3)")
                            .arg(themeId)
                            .arg(gap)
                            .arg(kRequiredGap)));

    // The chevron must survive the erase-before-draw re-ordering.
    QVERIFY2(rightmostInkColumn(image, chevronLeft, item.right() - kArrowRightInset, top,
                                bottom, background) >= chevronLeft,
             qPrintable(QStringLiteral("%1: chevron was erased").arg(themeId)));

    menu->hide();
}
