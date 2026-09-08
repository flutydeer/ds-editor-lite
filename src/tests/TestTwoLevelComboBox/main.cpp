#include "UI/Controls/TwoLevelComboBox.h"

#include <QtTest/QTest>
#include <QVersionNumber>

class TwoLevelComboBoxTests final : public QObject {
    Q_OBJECT

private slots:

    void mixDisplay_data() {
        QTest::addColumn<bool>("inherited");
        QTest::newRow("follow-track") << true;
        QTest::newRow("explicit-mix") << false;
    }

    void mixDisplay() {
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

    void selectSinger_data() {
        QTest::addColumn<bool>("multipleSpeakers");
        QTest::newRow("single-speaker") << false;
        QTest::newRow("speaker-submenu") << true;
    }

    void selectSinger() {
        QFETCH(bool, multipleSpeakers);
        const SpeakerInfo speaker(QStringLiteral("internal_emb"),
                                  QStringLiteral("Configured Emb Name"));
        QList<SpeakerInfo> speakers{speaker};
        if (multipleSpeakers)
            speakers.append(SpeakerInfo(QStringLiteral("second_emb"), QStringLiteral("Second")));
        const SingerInfo singer(
            {QStringLiteral("singer-id"), QStringLiteral("package-id"), QVersionNumber(1, 0)},
            QStringLiteral("Configured Role Name"), speakers);
        const PackageInfo package(QStringLiteral("package-id"), QVersionNumber(1, 0), {}, {}, {},
                                  {}, {}, {}, {singer});
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
};

QTEST_MAIN(TwoLevelComboBoxTests)
#include "main.moc"
