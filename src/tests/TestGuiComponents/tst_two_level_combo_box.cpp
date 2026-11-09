#include "tst_gui_components.h"

#include "UI/Controls/TwoLevelComboBox.h"

#include <lite/GUI/Theme/ThemeLoader.h>

#include <QtTest/QTest>
#include <QVersionNumber>
#include <QColor>
#include <QDir>
#include <QHash>
#include <QImage>
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
    QCOMPARE(menu->property("hasSubMenu").toBool(), true);

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
    QCOMPARE(menu->property("hasSubMenu").toBool(), false);

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
    QCOMPARE(menu->property("hasSubMenu").toBool(), true);
    const int subMenuWidth = menu->sizeHint().width();
    QVERIFY2(subMenuWidth - plainWidth >= 30,
             qPrintable(QStringLiteral("stylesheet was not re-evaluated: %1 -> %2")
                            .arg(plainWidth)
                            .arg(subMenuWidth)));

    menu->hide();
    menu->clear();
    QCOMPARE(menu->property("hasSubMenu").toBool(), false);
}
