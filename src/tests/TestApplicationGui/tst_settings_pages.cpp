#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"

#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/History/HistoryManager.h>

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
    void openAppearancePage(AppOptionsDialog &panel) {
        panel.resize(920, 720);
        panel.show();
        panel.activateWindow();
        QTRY_VERIFY(panel.isVisible());
        QTRY_VERIFY(panel.isActiveWindow());
        auto *tabs = panel.findChild<QListWidget *>("AppOptionsDialogTabListWidget");
        QVERIFY(tabs);
        auto *appearance = tabs->item(static_cast<int>(AppOptionsGlobal::Appearance) - 1);
        QVERIFY(appearance);
        QTest::mouseClick(tabs->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tabs->visualItemRect(appearance).center());
        QTRY_COMPARE(tabs->currentItem(), appearance);
    }
}

void ApplicationGuiTests::appearanceInputsPersistAcrossReopening() {
    auto &runtime = *context->m_coreRuntime;
    const auto before = runtime.documentVersion();
    const auto snapshot = runtime.settings().getSettings();
    QVERIFY(snapshot);
    const auto original = snapshot.get().appearance;
    const auto restore = qScopeGuard([&] { runtime.settings().updateAppearance({}, original); });
    const bool enabled = !original.animationEnabled;
    const double scale = original.animationTimeScale == 1.75 ? 0.75 : 1.75;

    {
        AppOptionsDialog panel;
        openAppearancePage(panel);
        if (QTest::currentTestFailed())
            return;
        auto *animation = panel.findChild<SwitchButton *>("appearanceAnimationEnabled");
        auto *duration = panel.findChild<QLineEdit *>("appearanceAnimationTimeScale");
        QVERIFY(animation);
        QVERIFY(duration);
        QTRY_VERIFY(animation->isVisible());
        QTRY_VERIFY(duration->isVisible());
        QCOMPARE(animation->value(), original.animationEnabled);

        QTest::mouseClick(animation, Qt::LeftButton);
        QCOMPARE(animation->value(), enabled);
        QTest::mouseClick(duration, Qt::LeftButton);
        QTRY_VERIFY(duration->hasFocus());
        QApplication::clipboard()->setText(QLocale().toString(scale));
        QTest::keySequence(duration, QKeySequence::SelectAll);
        QTest::keySequence(duration, QKeySequence::Paste);
        QCOMPARE(QLocale().toDouble(duration->text()), scale);
        QVERIFY(duration->hasAcceptableInput());
        QSignalSpy committed(duration, &QLineEdit::editingFinished);
        QTest::keyClick(duration, Qt::Key_Tab);
        QCOMPARE(committed.size(), 1);

        const auto changed = runtime.settings().getSettings();
        QVERIFY(changed);
        QCOMPARE(changed.get().appearance.animationEnabled, enabled);
        QCOMPARE(changed.get().appearance.animationTimeScale, scale);
        QFile config(appOptions->configPath());
        QVERIFY(config.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const auto saved = QJsonDocument::fromJson(config.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const auto appearance = saved.object().value(QStringLiteral("appearance")).toObject();
        QCOMPARE(appearance.value(QStringLiteral("animationEnabled")).toBool(), enabled);
        QCOMPARE(appearance.value(QStringLiteral("animationTimeScale")).toDouble(), scale);
        panel.close();
    }

    AppOptionsDialog reopened;
    openAppearancePage(reopened);
    if (QTest::currentTestFailed())
        return;
    auto *animation = reopened.findChild<SwitchButton *>("appearanceAnimationEnabled");
    auto *duration = reopened.findChild<QLineEdit *>("appearanceAnimationTimeScale");
    QVERIFY(animation);
    QVERIFY(duration);
    QCOMPARE(animation->value(), enabled);
    QCOMPARE(QLocale().toDouble(duration->text()), scale);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
