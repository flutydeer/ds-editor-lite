#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixBar.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixDialog.h"
#include "UI/Dialogs/SpeakerMix/SpeakerMixList.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/TagButton.h>
#include <lite/History/HistoryManager.h>

#include <QJsonArray>
#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
    QJsonArray savedPresets() {
        QJsonArray result;
        for (const auto &preset : SpeakerMixPresetStore::allPresets())
            result.append(preset.toJson());
        return result;
    }

    TagButton *speakerTag(SpeakerMixDialog &dialog, const QString &id) {
        for (auto *tag : dialog.findChildren<TagButton *>()) {
            if (tag->property("speakerName").toString() == id)
                return tag;
        }
        return nullptr;
    }
}

void ApplicationGuiTests::speakerMixSelectionAndDrag_data() {
    QTest::addColumn<bool>("accept");
    QTest::newRow("accept-edited-mix") << true;
    QTest::newRow("cancel-edited-mix") << false;
}

void ApplicationGuiTests::speakerMixSelectionAndDrag() {
    QFETCH(bool, accept);
    using namespace SpeakerMixModel;
    const SingerInfo singer(
        {QStringLiteral("mix-test"), QStringLiteral("gui-tests"), QVersionNumber(1, 0)},
        QStringLiteral("Mix Test"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm")),
         SpeakerInfo(QStringLiteral("air"), QStringLiteral("Air"))});
    const auto before = context->m_coreRuntime->documentVersion();
    const auto presets = savedPresets();
    SpeakerMixDialog dialog(singer, {});
    auto *list = dialog.findChild<SpeakerMixList *>();
    auto *air = speakerTag(dialog, QStringLiteral("air"));
    QVERIFY(list);
    QVERIFY(air);
    auto *bar = list->getMixBar();
    QVERIFY(bar);
    QSignalSpy finished(&dialog, &QDialog::finished);
    QSignalSpy valuesChanged(bar, &SpeakerMixBar::valuesChanged);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(air->isVisible());
    QTRY_VERIFY(bar->isVisible());
    QVERIFY(air->isChecked());

    QTest::mouseClick(air, Qt::LeftButton);
    QVERIFY(!air->isChecked());
    QCOMPARE(list->getLabels(),
             QVector<QString>({QStringLiteral("bright"), QStringLiteral("warm")}));
    // Removing a source preserves the remaining 34:33 ratio.
    QCOMPARE(list->getValues(), QVector<int>({51, 49}));
    const QPoint divider(1 + qRound((bar->width() - 2) * bar->getDoubleValues().first() / 100.0),
                         bar->height() / 2);
    const QPoint destination(1 + qRound((bar->width() - 2) * 0.70), divider.y());
    QTest::mousePress(bar, Qt::LeftButton, Qt::NoModifier, divider);
    QTest::mouseMove(bar, destination);
    QTest::mouseRelease(bar, Qt::LeftButton, Qt::NoModifier, destination);
    QVERIFY(!valuesChanged.isEmpty());
    QCOMPARE(bar->getValues(), QVector<int>({70, 30}));
    QCOMPARE(list->getValues(), QVector<int>({70, 30}));

    if (accept)
        QTest::mouseClick(dialog.okButton(), Qt::LeftButton);
    else
        QTest::mouseClick(dialog.cancelButton(), Qt::LeftButton);
    QTRY_COMPARE(finished.size(), 1);
    QCOMPARE(finished.first().first().toInt(), accept ? QDialog::Accepted : QDialog::Rejected);
    QVERIFY(!dialog.isVisible());
    if (accept) {
        const auto result = dialog.speakerMixData();
        QCOMPARE(result.mode, SingerSourceMode::FixedMix);
        QCOMPARE(result.sources.size(), 2);
        QCOMPARE(result.sources[0].speaker.id(), QStringLiteral("bright"));
        QCOMPARE(result.sources[1].speaker.id(), QStringLiteral("warm"));
        QCOMPARE(result.fixedWeights, QVector<double>({0.7}));
    } else {
        SpeakerMixDialog reopened(singer, {});
        auto *reopenedList = reopened.findChild<SpeakerMixList *>();
        QVERIFY(reopenedList);
        QCOMPARE(reopenedList->getLabels(),
                 QVector<QString>(
                     {QStringLiteral("bright"), QStringLiteral("warm"), QStringLiteral("air")}));
        QCOMPARE(reopenedList->getValues(), QVector<int>({34, 33, 33}));
    }
    QCOMPARE(savedPresets(), presets);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
