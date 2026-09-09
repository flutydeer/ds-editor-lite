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
#include <QApplication>
#include <QComboBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScopeGuard>
#include <QTimer>
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

    QPushButton *mixButton(QWidget &dialog, const QString &text) {
        for (auto *button : dialog.findChildren<QPushButton *>()) {
            if (button->text() == text)
                return button;
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

void ApplicationGuiTests::speakerMixPresetsFollowSaveSelectAndDeleteInputs() {
    const SingerInfo singer(
        {QStringLiteral("preset-test"), QStringLiteral("gui-tests"), QVersionNumber(1, 0)},
        QStringLiteral("Preset Test"),
        {SpeakerInfo(QStringLiteral("bright"), QStringLiteral("Bright")),
         SpeakerInfo(QStringLiteral("warm"), QStringLiteral("Warm")),
         SpeakerInfo(QStringLiteral("air"), QStringLiteral("Air"))});
    const auto before = context->m_coreRuntime->documentVersion();
    const auto originalPresets = savedPresets();
    QString savedId;
    const auto cleanup = qScopeGuard([&] {
        if (!savedId.isEmpty())
            SpeakerMixPresetStore::deletePreset(savedId);
    });
    SpeakerMixDialog dialog(singer, {});
    auto *presets = dialog.findChild<QComboBox *>();
    auto *save = mixButton(dialog, SpeakerMixDialog::tr("Save As"));
    auto *more = mixButton(dialog, SpeakerMixDialog::tr("More..."));
    auto *list = dialog.findChild<SpeakerMixList *>();
    QVERIFY(presets);
    QVERIFY(save);
    QVERIFY(more);
    QVERIFY(list);
    dialog.show();
    dialog.activateWindow();
    QTRY_VERIFY(save->isVisible());

    const auto saveAs = [&](bool accept) {
        bool visited = false;
        QTimer input;
        input.setSingleShot(true);
        connect(&input, &QTimer::timeout, &dialog, [&] {
            auto *prompt = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
            QVERIFY(prompt);
            const auto close = qScopeGuard([&] {
                if (prompt->isVisible())
                    prompt->reject();
            });
            auto *editor = prompt->findChild<QLineEdit *>();
            QVERIFY(editor);
            QTest::mouseClick(editor, Qt::LeftButton);
            QTest::keyClicks(editor, "Saved GUI mix");
            QTest::keyClick(editor, accept ? Qt::Key_Return : Qt::Key_Escape);
            visited = true;
        });
        input.start(0);
        QTest::mouseClick(save, Qt::LeftButton);
        QVERIFY(visited);
    };
    saveAs(false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(savedPresets(), originalPresets);
    saveAs(true);
    if (QTest::currentTestFailed())
        return;
    savedId = presets->currentData().toString();
    QVERIFY(!savedId.isEmpty());
    const auto saved = SpeakerMixPresetStore::findPreset(savedId);
    QVERIFY(saved);
    QCOMPARE(saved->name, QStringLiteral("Saved GUI mix"));
    QCOMPARE(saved->sources.size(), 3);

    auto *air = speakerTag(dialog, QStringLiteral("air"));
    QVERIFY(air);
    QTest::mouseClick(air, Qt::LeftButton);
    QTest::mouseClick(dialog.okButton(), Qt::LeftButton);
    const auto edited = dialog.speakerMixData();
    QCOMPARE(edited.sourcePresetId, savedId);
    QVERIFY(edited.sourcePresetDirty);
    QCOMPARE(edited.sources.size(), 2);

    SpeakerMixDialog reopened(singer, edited);
    presets = reopened.findChild<QComboBox *>();
    list = reopened.findChild<SpeakerMixList *>();
    more = mixButton(reopened, SpeakerMixDialog::tr("More..."));
    QVERIFY(presets);
    QVERIFY(list);
    QVERIFY(more);
    reopened.show();
    reopened.activateWindow();
    QTRY_VERIFY(presets->isVisible());
    QCOMPARE(presets->currentData().toString(), savedId);
    QTest::mouseClick(presets, Qt::LeftButton);
    QTest::keyClick(presets, Qt::Key_Home);
    QTest::keyClick(presets, Qt::Key_Return);
    QCOMPARE(presets->currentIndex(), 0);
    QTest::mouseClick(presets, Qt::LeftButton);
    QTest::keyClick(presets, Qt::Key_End);
    QTest::keyClick(presets, Qt::Key_Return);
    QCOMPARE(presets->currentData().toString(), savedId);
    QCOMPARE(list->getLabels().size(), 3);
    QCOMPARE(list->getValues(), QVector<int>({34, 33, 33}));

    const auto deletePreset = [&](QMessageBox::StandardButton response) {
        bool visited = false;
        QTimer confirmation;
        confirmation.setSingleShot(true);
        connect(&confirmation, &QTimer::timeout, &reopened, [&] {
            auto *prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(prompt);
            const auto close = qScopeGuard([&] {
                if (prompt->isVisible())
                    prompt->reject();
            });
            auto *button = prompt->button(response);
            QVERIFY(button);
            QTest::mouseClick(button, Qt::LeftButton);
            visited = true;
        });
        QTimer choose;
        choose.setSingleShot(true);
        connect(&choose, &QTimer::timeout, &reopened, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            const auto close = qScopeGuard([&] { menu->close(); });
            for (auto *action : menu->actions()) {
                if (action->text() != SpeakerMixDialog::tr("Delete"))
                    continue;
                QVERIFY(action->isEnabled());
                confirmation.start(0);
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(action).center());
                return;
            }
            QFAIL("Delete action was not found");
        });
        choose.start(0);
        QTest::mouseClick(more, Qt::LeftButton);
        QVERIFY(visited);
    };
    deletePreset(QMessageBox::No);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(SpeakerMixPresetStore::findPreset(savedId));
    QCOMPARE(presets->currentData().toString(), savedId);
    deletePreset(QMessageBox::Yes);
    if (QTest::currentTestFailed())
        return;
    QVERIFY(!SpeakerMixPresetStore::findPreset(savedId));
    QCOMPARE(presets->currentIndex(), 0);
    QCOMPARE(savedPresets(), originalPresets);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
