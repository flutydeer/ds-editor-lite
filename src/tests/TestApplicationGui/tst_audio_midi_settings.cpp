#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Audio/AudioSettings.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Audio/utils/SettingPagesSynthHelper.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Dialogs/Options/Pages/AudioPage.h"
#include "UI/Dialogs/Options/Pages/MidiPage.h"

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/History/HistoryManager.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include <QApplication>
#include <QClipboard>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <cmath>

namespace {
    void openPage(AppOptionsDialog &panel, AppOptionsGlobal::Option option) {
        panel.resize(920, 720);
        panel.show();
        panel.activateWindow();
        QTRY_VERIFY(panel.isActiveWindow());
        auto *tabs = panel.findChild<QListWidget *>("AppOptionsDialogTabListWidget");
        QVERIFY(tabs);
        auto *item = tabs->item(static_cast<int>(option) - 1);
        QVERIFY(item);
        QTest::mouseClick(tabs->viewport(), Qt::LeftButton, Qt::NoModifier,
                          tabs->visualItemRect(item).center());
        QCOMPARE(tabs->currentItem(), item);
    }

    template <typename SpinBox>
    void enterNumber(IOptionPage &page, SpinBox *spin, const QString &text) {
        QVERIFY(spin);
        page.ensureWidgetVisible(spin);
        QCoreApplication::processEvents();
        QVERIFY(spin->isEnabled());
        auto *editor = spin->template findChild<QLineEdit *>();
        QVERIFY(editor);
        QTest::mouseClick(editor, Qt::LeftButton);
        QTRY_VERIFY(editor->hasFocus());
        QApplication::clipboard()->setText(text);
        QTest::keySequence(editor, QKeySequence::SelectAll);
        QTest::keySequence(editor, QKeySequence::Paste);
        QTest::keyClick(editor, Qt::Key_Tab);
    }

    void chooseValue(IOptionPage &page, ComboBox *combo, int value) {
        QVERIFY(combo);
        const auto index = combo->findData(value);
        QVERIFY(index >= 0);
        page.ensureWidgetVisible(combo);
        QCoreApplication::processEvents();
        combo->setFocus();
        QTRY_VERIFY(combo->hasFocus());
        QTest::keyClick(combo, Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(combo, Qt::Key_Down);
        QCOMPARE(combo->currentData().toInt(), value);
    }
}

void ApplicationGuiTests::audioPageInputsPersistWithoutPlayback() {
    auto &runtime = *context->m_coreRuntime;
    const auto original = runtime.settings().getSettings();
    QVERIFY(original);
    auto *output = AudioSystem::outputSystem();
    auto *outputContext = output->outputContext();
    QVERIFY(!output->isReady());
    auto *mixer = outputContext->controlMixer();
    const auto originalGain = mixer->gain();
    const auto originalPan = mixer->pan();
    const auto originalReadAhead = output->fileBufferingReadAheadSize();
    const auto originalHotPlug = outputContext->hotPlugNotificationMode();
    const auto restore = qScopeGuard([&] {
        mixer->setGain(originalGain);
        mixer->setPan(originalPan);
        output->setFileBufferingReadAheadSize(originalReadAhead);
        output->setHotPlugNotificationMode(originalHotPlug);
        QVERIFY(runtime.settings().updateAudio({}, original.get().audio));
    });
    const auto before = runtime.documentVersion();
    const auto *beforeUndo = historyManager->nextUndoEntry();
    const auto readAhead = originalReadAhead == 4096 ? 8192 : 4096;
    const auto playhead = original.get().audio.playheadBehavior == 1 ? 2 : 1;
    QSignalSpy readAheadChanged(output, &AbstractOutputSystem::fileBufferingReadAheadSizeChanged);
    {
        AppOptionsDialog panel;
        openPage(panel, AppOptionsGlobal::Audio);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<AudioPage *>();
        QVERIFY(page);
        auto *device = page->findChild<ComboBox *>("audioDevice");
        auto *buffer = page->findChild<ComboBox *>("audioBufferSize");
        auto *rate = page->findChild<ComboBox *>("audioSampleRate");
        auto *test = page->findChild<QPushButton *>("audioDeviceTest");
        auto *controlPanel = page->findChild<QPushButton *>("audioDeviceControlPanel");
        QVERIFY(device && buffer && rate && test && controlPanel);
        QVERIFY(!test->isEnabled());
        if (!outputContext->driver()) {
            QCOMPARE(device->count(), 0);
            QCOMPARE(buffer->count(), 0);
            QCOMPARE(rate->count(), 0);
            QVERIFY(!device->isEnabled());
            QVERIFY(!buffer->isEnabled());
            QVERIFY(!rate->isEnabled());
            QVERIFY(!controlPanel->isEnabled());
            auto *driver = page->findChild<ComboBox *>("audioDriver");
            QVERIFY(driver && driver->isEnabled());
        }
        auto *gain = page->findChild<SVS::ExpressionDoubleSpinBox *>("audioDeviceGain");
        auto *gainSlider = page->findChild<SVS::SeekBar *>("audioDeviceGainSlider");
        auto *pan = page->findChild<SVS::ExpressionSpinBox *>("audioDevicePan");
        auto *panSlider = page->findChild<SVS::SeekBar *>("audioDevicePanSlider");
        auto *hotPlug = page->findChild<ComboBox *>("audioHotPlugMode");
        auto *playheadChoice = page->findChild<ComboBox *>("audioPlayheadBehavior");
        auto *fileBuffer = page->findChild<SVS::ExpressionSpinBox *>("audioFileReadAhead");
        QVERIFY(gain && gainSlider && pan && panSlider && hotPlug && playheadChoice && fileBuffer);
        enterNumber(*page, gain, QLocale().toString(-6.0));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(gain->value(), -6.0);
        QVERIFY(std::abs(gainSlider->displayValue() + 6.0) < 1e-4);
        QVERIFY(std::abs(mixer->gain() - 0.50118723f) < 1e-6f);
        enterNumber(*page, pan, QStringLiteral("25"));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(panSlider->value(), 25.0);
        QCOMPARE(mixer->pan(), 0.25f);
        page->ensureWidgetVisible(panSlider);
        QTest::mouseClick(panSlider, Qt::LeftButton, Qt::NoModifier,
                          QPoint(panSlider->width() * 5 / 8, panSlider->height() / 2));
        panSlider->setFocus();
        QTRY_VERIFY(panSlider->hasFocus());
        QTest::keyClick(panSlider, Qt::Key_Right);
        QCOMPARE(pan->value(), 26);
        QCOMPARE(mixer->pan(), 0.26f);
        chooseValue(*page, hotPlug, talcs::OutputContext::None);
        chooseValue(*page, playheadChoice, playhead);
        enterNumber(*page, fileBuffer, QString::number(readAhead));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(output->fileBufferingReadAheadSize(), originalReadAhead);
        QVERIFY(readAheadChanged.isEmpty());
        panel.close();
    }
    QCOMPARE(output->fileBufferingReadAheadSize(), qint64(readAhead));
    QCOMPARE(outputContext->hotPlugNotificationMode(), talcs::OutputContext::None);
    QCOMPARE(readAheadChanged.count(), 1);
    const auto applied = runtime.settings().getSettings();
    QVERIFY(applied);
    QVERIFY(std::abs(applied.get().audio.deviceGain - 0.50118723) < 1e-6);
    QVERIFY(std::abs(applied.get().audio.devicePan - 0.26) < 1e-6);
    QCOMPARE(applied.get().audio.fileBufferingReadAheadSize, qint64(readAhead));
    QCOMPARE(applied.get().audio.playheadBehavior, playhead);
    {
        AppOptions stored;
        const auto &audio = stored.audio()->obj;
        QVERIFY(std::abs(audio.value("deviceGain").toDouble() - 0.50118723) < 1e-6);
        QVERIFY(std::abs(audio.value("devicePan").toDouble() - 0.26) < 1e-6);
        QCOMPARE(audio.value("fileBufferingReadAheadSize").toInteger(), qint64(readAhead));
        QCOMPARE(audio.value("playheadBehavior").toInt(), playhead);
        QCOMPARE(audio.value("hotPlugNotificationMode").toInt(), int(talcs::OutputContext::None));
    }
    {
        AppOptionsDialog reopened;
        openPage(reopened, AppOptionsGlobal::Audio);
        if (QTest::currentTestFailed())
            return;
        auto *gain = reopened.findChild<SVS::ExpressionDoubleSpinBox *>("audioDeviceGain");
        auto *pan = reopened.findChild<SVS::ExpressionSpinBox *>("audioDevicePan");
        auto *buffer = reopened.findChild<SVS::ExpressionSpinBox *>("audioFileReadAhead");
        auto *behavior = reopened.findChild<ComboBox *>("audioPlayheadBehavior");
        QVERIFY(gain && pan && buffer && behavior);
        QCOMPARE(gain->value(), -6.0);
        QCOMPARE(pan->value(), 26);
        QCOMPARE(buffer->value(), readAhead);
        QCOMPARE(behavior->currentData().toInt(), playhead);
    }
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
    QVERIFY(!output->isReady());
}

void ApplicationGuiTests::midiPageSynthInputsPersistWithoutPlayback() {
    auto &runtime = *context->m_coreRuntime;
    const auto original = runtime.settings().getSettings();
    QVERIFY(original);
    QVERIFY(!AudioSystem::outputSystem()->isReady());
    auto *midi = AudioSystem::midiSystem();
    auto *synthesizer = midi->synthesizer()->noteSynthesizer();
    const auto originalFrequency = midi->synthesizer()->frequencyOfA();
    const auto originalAttack = synthesizer->attackTime();
    const auto originalDecay = synthesizer->decayTime();
    const auto originalRelease = synthesizer->releaseTime();
    const auto originalRatio = synthesizer->decayRatio();
    const auto restore = qScopeGuard([&] {
        const auto &audio = original.get().audio;
        midi->setGenerator(audio.midiSynthesizerGenerator);
        midi->setAmplitudeDecibel(audio.midiSynthesizerAmplitude);
        midi->setAttackMsec(audio.midiSynthesizerAttackMilliseconds);
        midi->setDecayMsec(audio.midiSynthesizerDecayMilliseconds);
        midi->setDecayRatio(audio.midiSynthesizerDecayRatio);
        midi->setReleaseMsec(audio.midiSynthesizerReleaseMilliseconds);
        midi->setFrequencyOfA(audio.midiSynthesizerFrequencyOfA);
        midi->synthesizer()->setFrequencyOfA(originalFrequency);
        synthesizer->setAttackTime(originalAttack);
        synthesizer->setDecayTime(originalDecay);
        synthesizer->setReleaseTime(originalRelease);
        synthesizer->setDecayRatio(originalRatio);
        QVERIFY(runtime.settings().updateAudio({}, audio));
    });
    const auto before = runtime.documentVersion();
    const auto *beforeUndo = historyManager->nextUndoEntry();
    {
        AppOptionsDialog panel;
        openPage(panel, AppOptionsGlobal::Midi);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<MidiPage *>();
        QVERIFY(page);
        auto *generator = page->findChild<ComboBox *>("midiGenerator");
        auto *amplitude = page->findChild<SVS::ExpressionDoubleSpinBox *>("midiAmplitude");
        auto *attack = page->findChild<SVS::ExpressionSpinBox *>("midiAttack");
        auto *decay = page->findChild<SVS::ExpressionSpinBox *>("midiDecay");
        auto *ratio = page->findChild<SVS::ExpressionDoubleSpinBox *>("midiDecayRatio");
        auto *release = page->findChild<SVS::ExpressionSpinBox *>("midiRelease");
        auto *frequency = page->findChild<SVS::ExpressionDoubleSpinBox *>("midiFrequencyOfA");
        auto *adjust = page->findChild<SwitchButton *>("midiAdjustByProject");
        auto *preview = page->findChild<QPushButton *>("midiPreview");
        auto *helper = page->findChild<SettingPageSynthHelper *>();
        QVERIFY(generator && amplitude && attack && decay && ratio && release && frequency &&
                adjust && preview && helper);
        chooseValue(*page, generator, talcs::NoteSynthesizer::Sine);
        enterNumber(*page, amplitude, QLocale().toString(-12.0));
        enterNumber(*page, attack, QStringLiteral("15"));
        enterNumber(*page, decay, QStringLiteral("250"));
        enterNumber(*page, ratio, QLocale().toString(0.35));
        enterNumber(*page, release, QStringLiteral("25"));
        if (QTest::currentTestFailed())
            return;
        if (adjust->value()) {
            page->ensureWidgetVisible(adjust);
            QTest::mouseClick(adjust, Qt::LeftButton);
        }
        QVERIFY(!adjust->value());
        enterNumber(*page, frequency, QLocale().toString(442.0));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(amplitude->value(), -12.0);
        QCOMPARE(attack->value(), 15);
        QCOMPARE(decay->value(), 250);
        QCOMPARE(ratio->value(), 0.35);
        QCOMPARE(release->value(), 25);
        QCOMPARE(frequency->value(), 442.0);
        QVERIFY(std::abs(helper->m_testMixer.gain() - 0.25118864f) < 1e-6f);
        QCOMPARE(helper->m_testSynthesizer.decayRatio(), 0.35);
        const auto unchanged = runtime.settings().getSettings();
        QVERIFY(unchanged);
        QCOMPARE(unchanged.get().audio.midiSynthesizerGenerator,
                 original.get().audio.midiSynthesizerGenerator);
        QCOMPARE(unchanged.get().audio.midiSynthesizerAttackMilliseconds,
                 original.get().audio.midiSynthesizerAttackMilliseconds);
        page->ensureWidgetVisible(preview);
        QTest::mouseClick(preview, Qt::LeftButton);
        QVERIFY(!preview->isChecked());
        QVERIFY(helper->isTestFinished.loadAcquire());
        panel.close();
    }
    QCOMPARE(midi->generator(), int(talcs::NoteSynthesizer::Sine));
    QCOMPARE(midi->amplitudeDecibel(), -12.0);
    QCOMPARE(midi->attackMsec(), 15);
    QCOMPARE(midi->decayMsec(), 250);
    QCOMPARE(midi->decayRatio(), 0.35);
    QCOMPARE(midi->releaseMsec(), 25);
    QCOMPARE(midi->frequencyOfA(), 442.0);
    QCOMPARE(synthesizer->attackTime(), qint64{720});
    QCOMPARE(synthesizer->decayTime(), qint64{12000});
    QCOMPARE(synthesizer->releaseTime(), qint64{1200});
    QCOMPARE(synthesizer->decayRatio(), 0.35);
    QCOMPARE(midi->synthesizer()->frequencyOfA(), 442.0);
    {
        AppOptions stored;
        const auto &audio = stored.audio()->obj;
        QCOMPARE(audio.value("midiSynthesizerGenerator").toInt(),
                 int(talcs::NoteSynthesizer::Sine));
        QCOMPARE(audio.value("midiSynthesizerAmplitude").toDouble(), -12.0);
        QCOMPARE(audio.value("midiSynthesizerAttackMsec").toInt(), 15);
        QCOMPARE(audio.value("midiSynthesizerDecayMsec").toInt(), 250);
        QCOMPARE(audio.value("midiSynthesizerDecayRatio").toDouble(), 0.35);
        QCOMPARE(audio.value("midiSynthesizerReleaseMsec").toInt(), 25);
        QCOMPARE(audio.value("midiSynthesizerFrequencyOfA").toDouble(), 442.0);
    }
    {
        AppOptionsDialog reopened;
        openPage(reopened, AppOptionsGlobal::Midi);
        if (QTest::currentTestFailed())
            return;
        auto *generator = reopened.findChild<ComboBox *>("midiGenerator");
        auto *attack = reopened.findChild<SVS::ExpressionSpinBox *>("midiAttack");
        auto *frequency = reopened.findChild<SVS::ExpressionDoubleSpinBox *>("midiFrequencyOfA");
        auto *adjust = reopened.findChild<SwitchButton *>("midiAdjustByProject");
        QVERIFY(generator && attack && frequency && adjust);
        QCOMPARE(generator->currentData().toInt(), int(talcs::NoteSynthesizer::Sine));
        QCOMPARE(attack->value(), 15);
        QCOMPARE(frequency->value(), 442.0);
        QVERIFY(!adjust->value());
    }
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
    QVERIFY(!AudioSystem::outputSystem()->isReady());
}
