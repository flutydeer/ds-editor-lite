#include "tst_gui_components.h"

#include "UI/Controls/TwoLevelComboBox.h"
#include "UI/Controls/SingerMenuUnavailablePackages.h"

#include <QtTest/QTest>
#include <QVersionNumber>

void GuiComponentTests::mixDisplay_data() {
    QTest::addColumn<bool>("inherited");
    QTest::newRow("follow-track") << true;
    QTest::newRow("explicit-mix") << false;
}

void GuiComponentTests::unavailablePackagesStayVisibleAcrossMenuRefresh() {
    const auto reason = QStringLiteral("Dependency wolf/lang-jpn could not be resolved");
    const auto failed = PackageInfo::unavailable(QStringLiteral("voicebanks/fixture"), reason);
    QVERIFY(failed.isUnavailable() && !failed.isEmpty());
    QCOMPARE(failed.unavailableReason(), reason);
    TwoLevelComboBox box;
    int opened = 0;
    connect(&box, &TwoLevelComboBox::itemsPopulated, &box, [&] {
        SingerMenuUnavailablePackages::append(
            box.mainMenu(), {GetInstalledPackagesResult::FailedPackage(failed.path(), reason)},
            [&] { ++opened; });
    });
    for (int refresh = 0; refresh < 2; ++refresh) {
        box.setItems({failed});
        const auto actions = box.mainMenu()->actions();
        QCOMPARE(actions.size(), 4);
        QVERIFY(!actions.at(1)->isEnabled());
        QCOMPARE(actions.at(1)->toolTip(), reason);
        QVERIFY(actions.at(2)->isSeparator());
        QVERIFY(actions.at(3)->isEnabled());
        QVERIFY(box.mainMenu()->toolTipsVisible());
        actions.at(3)->trigger();
        QCOMPARE(opened, refresh + 1);
    }
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
