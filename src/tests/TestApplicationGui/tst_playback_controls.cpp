#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/PlaybackController.h"
#include "UI/Views/Common/TempoEditWidget.h"
#include "UI/Views/MainTitleBar/PlaybackView.h"
#include "UI/Views/MainTitleBar/TempoComboBox.h"
#include "UI/Views/MainTitleBar/TempoPopupWidget.h"
#include "UI/Views/MainTitleBar/TimeSignatureComboBox.h"
#include "UI/Views/Common/TimeSignaturePopupWidget.h"

#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>
#include <lite/GUI/Controls/TapTempoButton.h>
#include <lite/History/HistoryManager.h>
#include <lite/MusicBase/Tempo.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QApplication>
#include <QClipboard>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

void ApplicationGuiTests::playbackTextInputsValidateCommitAndCancel() {
    auto &runtime = *context->m_coreRuntime;
    playbackController->setPosition(0);
    PlaybackView controls;
    controls.show();
    controls.activateWindow();
    QTRY_VERIFY(controls.isActiveWindow());
    auto *tempo = controls.findChild<TempoComboBox *>();
    auto *signature = controls.findChild<TimeSignatureComboBox *>();
    auto *position = controls.findChild<InlineEditLabel *>("elTime");
    QVERIFY(tempo && signature && position);
    historyManager->reset();
    const auto original = context->m_appModel->serialize();
    const auto before = runtime.documentVersion();
    const auto enter = [&](InlineEditLabel *label, const QString &value,
                           Qt::Key finish = Qt::Key_Return) {
        QTest::mouseDClick(label, Qt::LeftButton);
        QTRY_VERIFY(qobject_cast<QLineEdit *>(QApplication::focusWidget()));
        auto *input = qobject_cast<QLineEdit *>(QApplication::focusWidget());
        QApplication::clipboard()->setText(value);
        QTest::keySequence(input, QKeySequence::SelectAll);
        QTest::keySequence(input, QKeySequence::Paste);
        QTest::keyClick(input, finish);
    };
    enter(tempo, QLocale().toString(132.5));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(context->m_appModel->timeline().tempoAt(0), 132.5);
    QCOMPARE(tempo->text(), Tempo::formatValue(132.5));
    const auto tempoEdit = runtime.documentVersion();
    enter(tempo, QStringLiteral("-1"));
    enter(tempo, QStringLiteral("160"), Qt::Key_Escape);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), tempoEdit);
    QCOMPARE(tempo->text(), Tempo::formatValue(132.5));

    const auto editedSignature = QStringLiteral("%L1/%L2").arg(7).arg(8);
    enter(signature, editedSignature);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(context->m_appModel->timeline().timeSignatureAt(0).numerator, 7);
    QCOMPARE(context->m_appModel->timeline().timeSignatureAt(0).denominator, 8);
    QCOMPARE(signature->text(), editedSignature);
    const auto signatureEdit = runtime.documentVersion();
    enter(signature, QStringLiteral("7/3"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), signatureEdit);
    QCOMPARE(signature->text(), editedSignature);
    QCOMPARE(signatureEdit.revision, before.revision + 2);

    const auto destination = context->m_appModel->timeline().getBarBeatTickTime(1920);
    enter(position, destination);
    if (QTest::currentTestFailed())
        return;
    // The audio transport snaps the requested position to a sample boundary.
    QVERIFY(qAbs(playbackController->position() - 1920.0) < 1.0);
    QCOMPARE(position->text(), destination);
    QCOMPARE(runtime.documentVersion(), signatureEdit);
    QVERIFY(runtime.history().undo(commandContext()));
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(context->m_appModel->serialize(), original);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::playbackPopupsEditTheMarkerChosenWhenTheyOpen() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.timeline().setTempo(commandContext(), 0, 120));
    QVERIFY(runtime.timeline().setTempo(commandContext(), 1920, 90));
    QVERIFY(runtime.timeline().setTimeSignature(commandContext(), 2, 3, 4));
    playbackController->setPosition(0);
    PlaybackView controls;
    controls.show();
    controls.activateWindow();
    QTRY_VERIFY(controls.isActiveWindow());
    auto *tempo = controls.findChild<TempoComboBox *>();
    auto *signature = controls.findChild<TimeSignatureComboBox *>();
    QVERIFY(tempo && signature);
    auto *tempoPopup = tempo->findChild<TempoPopupWidget *>();
    auto *signaturePopup = signature->findChild<TimeSignaturePopupWidget *>();
    QVERIFY(tempoPopup && signaturePopup);
    const auto closePopups = qScopeGuard([&] {
        tempoPopup->close();
        signaturePopup->close();
    });
    historyManager->reset();
    const auto before = runtime.documentVersion();

    QTest::mouseClick(tempo, Qt::LeftButton);
    QTRY_VERIFY(tempoPopup->isVisible());
    auto *spin = tempoPopup->findChild<SVS::ExpressionDoubleSpinBox *>("spinTempo");
    QVERIFY(spin);
    QCOMPARE(spin->value(), 120.0);
    playbackController->setPosition(1920);
    QCOMPARE(tempo->text(), Tempo::formatValue(90));
    QTest::mouseClick(spin, Qt::LeftButton);
    QTest::keySequence(spin, QKeySequence::SelectAll);
    QTest::keyClicks(spin, "150");
    QTest::keyClick(spin, Qt::Key_Return);
    QCOMPARE(context->m_appModel->timeline().tempoAt(0), 150.0);
    QCOMPARE(context->m_appModel->timeline().tempoAt(1920), 90.0);
    QCOMPARE(tempo->text(), Tempo::formatValue(90));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QTest::keyClick(tempoPopup, Qt::Key_Escape);
    QTRY_VERIFY(!tempoPopup->isVisible());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(context->m_appModel->timeline().tempoAt(0), 120.0);
    QCOMPARE(tempo->text(), Tempo::formatValue(90));

    playbackController->setPosition(0);
    QTest::mouseClick(signature, Qt::RightButton);
    QTRY_VERIFY(signaturePopup->isVisible());
    QPushButton *preset = nullptr;
    for (auto *button : signaturePopup->findChildren<QPushButton *>()) {
        if (button->text() == QStringLiteral("%L1/%L2").arg(6).arg(8))
            preset = button;
    }
    QVERIFY(preset);
    playbackController->setPosition(3840);
    const auto currentSignature = QStringLiteral("%L1/%L2").arg(3).arg(4);
    QCOMPARE(signature->text(), currentSignature);
    QTest::mouseClick(preset, Qt::LeftButton);
    const auto edited = context->m_appModel->timeline().timeSignatureAt(0);
    QCOMPARE(edited.numerator, 6);
    QCOMPARE(edited.denominator, 8);
    const auto untouched = context->m_appModel->timeline().timeSignatureAt(2);
    QCOMPARE(untouched.numerator, 3);
    QCOMPARE(untouched.denominator, 4);
    QCOMPARE(signature->text(), currentSignature);
    QTest::keyClick(signaturePopup, Qt::Key_Escape);
    QTRY_VERIFY(!signaturePopup->isVisible());
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(context->m_appModel->timeline().timeSignatureAt(0).numerator, 4);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::tapTempoMeasuresASequenceAndResetsAfterInactivity() {
    qint64 nowMs = 0;
    TempoEditWidget editor(nullptr, [&] { return nowMs; });
    editor.setTempo(120);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    auto *tap = editor.findChild<TapTempoButton *>("btnTapTempo");
    QVERIFY(tap);
    QSignalSpy changed(&editor, &TempoEditWidget::tempoChanged);
    QTest::mouseClick(tap, Qt::LeftButton);
    QCOMPARE(tap->text(), TempoEditWidget::tr("Keep Tapping"));
    QVERIFY(!tap->isStable());
    for (int i = 0; i < 16; ++i) {
        nowMs += 500;
        QTest::mouseClick(tap, Qt::LeftButton);
    }
    QVERIFY(tap->isStable());
    QCOMPARE(tap->progress(), 1.0);
    QVERIFY(tap->text().endsWith(QStringLiteral(" BPM")));
    QCOMPARE(tap->text(), QStringLiteral("%L1 BPM").arg(120));
    QTest::mouseClick(tap, Qt::LeftButton);
    QCOMPARE(tap->text(), QStringLiteral("%L1 BPM").arg(120));
    for (int i = 0; i < 32; ++i) {
        nowMs += 250;
        QTest::mouseClick(tap, Qt::LeftButton);
    }
    QCOMPARE(tap->text(), QStringLiteral("%L1 BPM").arg(240));
    QVERIFY(tap->isStable());
    QCOMPARE(editor.tempo(), 120.0);
    QVERIFY(changed.isEmpty());

    nowMs += 3100;
    QTest::mouseClick(tap, Qt::LeftButton);
    QCOMPARE(tap->text(), TempoEditWidget::tr("Keep Tapping"));
    QVERIFY(!tap->isStable());
    QCOMPARE(tap->progress(), 0.0);
    nowMs += 500;
    QTest::mouseClick(tap, Qt::LeftButton);
    QVERIFY(tap->progress() > 0);
    QVERIFY(!tap->isStable());
    editor.resetTapTempo();
    QCOMPARE(tap->text(), TempoEditWidget::tr("Tap Tempo"));
    QCOMPARE(tap->progress(), 0.0);
    QCOMPARE(editor.tempo(), 120.0);
    QVERIFY(changed.isEmpty());
}
