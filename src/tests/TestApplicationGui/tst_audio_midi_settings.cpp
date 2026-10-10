#include "tst_application_gui.h"
#include "../TestSupport/OptionsPanelFixture.h"
#include "../TestSupport/FileWriteBlocker.h"

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
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/History/HistoryManager.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDriverManager.h>
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
#include <QFile>
#include <QDir>
#include <QtTest/QTest>
#include <QAbstractItemView>
#include <QMessageBox>
#include <QTimer>

#include <cmath>
#include <memory>

using TestSupport::openOptionsPage;

namespace {
    class FixtureAudioDevice final : public talcs::AudioDevice {
    public:
        FixtureAudioDevice(talcs::AudioDriver *driver, const QString &name) : AudioDevice(driver) {
            setName(name);
            setDriver(driver);
            setChannelCount(2);
            setActiveChannelCount(2);
            setAvailableBufferSizes({256, 512, 1024});
            setPreferredBufferSize(512);
            setAvailableSampleRates({44100, 48000});
            setPreferredSampleRate(48000);
            setIsInitialized(true);
        }

        bool openControlPanel() override {
            ++controlPanelRequests;
            return true;
        }

        int controlPanelRequests = 0;
    };

    class FixtureAudioDriver final : public talcs::AudioDriver {
    public:
        explicit FixtureAudioDriver(bool defaultDeviceAvailable)
            : defaultDeviceAvailable(defaultDeviceAvailable) {
            setName(QStringLiteral("fixture-output"));
        }

        bool initialize() override {
            return available && AudioDriver::initialize();
        }

        QStringList devices() const override {
            return availableDevices;
        }

        QString defaultDevice() const override {
            return defaultDeviceAvailable ? devices().first() : QString();
        }

        talcs::AudioDevice *createDefaultDevice() override {
            return defaultDeviceAvailable ? new FixtureAudioDevice(this, {}) : nullptr;
        }

        talcs::AudioDevice *createDevice(const QString &name) override {
            return devices().contains(name) && name != unavailableDevice
                       ? new FixtureAudioDevice(this, name)
                       : nullptr;
        }

        QStringList availableDevices{QStringLiteral("Output A"), QStringLiteral("Output B")};
        bool available = true;
        QString unavailableDevice;

    private:
        const bool defaultDeviceAvailable;
    };

    template <typename SpinBox>
    void enterNumber(IOptionPage &page, SpinBox *spin, const QString &text) {
        QVERIFY(spin);
        page.ensureWidgetVisible(spin);
        QCoreApplication::processEvents();
        QVERIFY(spin->isEnabled());
        auto *editor = spin->template findChild<QLineEdit *>();
        QVERIFY(editor);
        QTest::mouseClick(editor, Qt::LeftButton);
        QTRY_VERIFY2(
            editor->hasFocus(),
            qPrintable(
                QStringLiteral("input=%1 focus=%2 popup=%3 visible-center=%4")
                    .arg(spin->objectName(),
                         QApplication::focusWidget() ? QApplication::focusWidget()->objectName()
                                                     : QStringLiteral("none"),
                         QApplication::activePopupWidget()
                             ? QString::fromLatin1(
                                   QApplication::activePopupWidget()->metaObject()->className())
                             : QStringLiteral("none"))
                    .arg(editor->visibleRegion().contains(editor->rect().center()))));
        QApplication::clipboard()->setText(text);
        QTest::keySequence(editor, QKeySequence::SelectAll);
        QTest::keySequence(editor, QKeySequence::Paste);
        QTest::keyClick(editor, Qt::Key_Tab);
    }

    void chooseValue(IOptionPage &page, ComboBox *combo, const QVariant &value,
                     bool expectSelected = true) {
        QVERIFY(combo);
        const auto index = combo->findData(value);
        QVERIFY(index >= 0);
        page.ensureWidgetVisible(combo);
        QCoreApplication::processEvents();
        QTest::mouseClick(combo, Qt::LeftButton);
        auto *items = combo->view();
        QTRY_VERIFY(items->isVisible());
        QTest::keyClick(items, Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(items, Qt::Key_Down);
        QTest::keyClick(items, Qt::Key_Return);
        if (expectSelected)
            QTRY_COMPARE(combo->currentIndex(), index);
        QTRY_VERIFY(!items->isVisible());
        // Offscreen popup activation can leave its host window without keyboard focus.
        combo->window()->activateWindow();
        QTRY_VERIFY(combo->window()->isActiveWindow());
    }

    void chooseUnavailableValue(IOptionPage &page, ComboBox *combo, const QVariant &value) {
        const auto name = combo->itemText(combo->findData(value));
        QVERIFY(!name.isEmpty());
        bool warned = false;
        QTimer dismiss;
        dismiss.setInterval(10);
        QObject::connect(&dismiss, &QTimer::timeout, &page, [&] {
            auto *warning = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!warning)
                return;
            dismiss.stop();
            const auto closeOnFailure = qScopeGuard([&] {
                if (warning->isVisible())
                    warning->reject();
            });
            QCOMPARE(warning->icon(), QMessageBox::Warning);
            QVERIFY(warning->text().contains(name));
            auto *button = warning->button(QMessageBox::Ok);
            QVERIFY(button);
            QTest::mouseClick(button, Qt::LeftButton);
            warned = true;
        });
        dismiss.start();
        chooseValue(page, combo, value, false);
        dismiss.stop();
        QVERIFY(warned);
    }
}

void ApplicationGuiTests::audioDeviceChoicesApplyThroughThePage_data() {
    QTest::addColumn<bool>("defaultDeviceAvailable");
    QTest::newRow("default-output") << true;
    QTest::newRow("named-outputs-only") << false;
}

void ApplicationGuiTests::audioDeviceChoicesApplyThroughThePage() {
    QFETCH(bool, defaultDeviceAvailable);
    auto &runtime = *context->m_coreRuntime;
    const auto original = runtime.settings().getSettings();
    QVERIFY(original);
    auto *output = AudioSystem::outputSystem();
    auto *deviceContext = output->outputContext();
    auto *manager = deviceContext->driverManager();
    const auto previousDriver =
        deviceContext->driver() ? deviceContext->driver()->name() : QString();
    const auto previousDevice =
        deviceContext->device() ? deviceContext->device()->name() : QString();
    const bool previousDeviceAvailable = deviceContext->device() != nullptr;
    const auto previousBuffer = deviceContext->adoptedBufferSize();
    const auto previousRate = deviceContext->adoptedSampleRate();
    auto driver = std::make_unique<FixtureAudioDriver>(defaultDeviceAvailable);
    const auto restore = qScopeGuard([&] {
        const auto removeDriver = qScopeGuard([&] {
            if (manager->driver(driver->name()) == driver.get())
                QVERIFY(manager->removeDriver(driver.get()));
        });
        deviceContext->setDriver({});
        deviceContext->setAdoptedBufferSize(previousBuffer);
        deviceContext->setAdoptedSampleRate(previousRate);
        if (!previousDriver.isEmpty()) {
            const bool restored = deviceContext->setDriver(
                previousDriver, talcs::OutputContext::DO_DoNotChangeAdoptedSpec);
            // An initialized driver can have no available output device.
            if (previousDeviceAvailable) {
                QVERIFY(restored);
                QVERIFY(deviceContext->setDevice(previousDevice,
                                                 talcs::OutputContext::DO_DoNotChangeAdoptedSpec));
            }
        }
        if (auto *device = deviceContext->device()) {
            device->stop();
            device->close();
        }
        QVERIFY(runtime.settings().updateAudio({}, original.get().audio));
    });
    QVERIFY(manager->addAudioDriver(driver.get()));
    const auto before = runtime.documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    {
        AppOptionsDialog panel;
        openOptionsPage(panel, AppOptionsGlobal::Audio);
        if (QTest::currentTestFailed())
            return;
        auto *page = panel.findChild<AudioPage *>();
        QVERIFY(page);
        auto *drivers = page->findChild<ComboBox *>("audioDriver");
        auto *devices = page->findChild<ComboBox *>("audioDevice");
        auto *buffer = page->findChild<ComboBox *>("audioBufferSize");
        auto *rate = page->findChild<ComboBox *>("audioSampleRate");
        auto *controlPanel = page->findChild<QPushButton *>("audioDeviceControlPanel");
        QVERIFY(drivers && devices && buffer && rate && controlPanel);
        const auto beforeDriver = runtime.settings().getSettings();
        QVERIFY(beforeDriver);
        driver->available = false;
        chooseUnavailableValue(*page, drivers, driver->name());
        if (QTest::currentTestFailed())
            return;
        QVERIFY(!deviceContext->driver());
        QVERIFY(drivers->currentData().isNull());
        QVERIFY(!devices->isEnabled() && !buffer->isEnabled() && !rate->isEnabled());
        QVERIFY(!controlPanel->isEnabled());
        const auto afterDriverFailure = runtime.settings().getSettings();
        QVERIFY(afterDriverFailure);
        QCOMPARE(afterDriverFailure.get().audio, beforeDriver.get().audio);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
        driver->available = true;
        chooseValue(*page, drivers, driver->name());
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(deviceContext->driver(), driver.get());
        QVERIFY(deviceContext->device() && deviceContext->device()->isOpen());
        QVERIFY(devices->isEnabled() && buffer->isEnabled() && rate->isEnabled());
        QCOMPARE(devices->currentData().toString(),
                 defaultDeviceAvailable ? QString() : QStringLiteral("Output A"));
        auto *previousOutput = deviceContext->device();
        const auto previousChoice = devices->currentData();
        const auto beforeDevice = runtime.settings().getSettings();
        QVERIFY(beforeDevice);
        driver->unavailableDevice = QStringLiteral("Output B");
        chooseUnavailableValue(*page, devices, driver->unavailableDevice);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(deviceContext->device(), previousOutput);
        QCOMPARE(devices->currentData(), previousChoice);
        const auto afterDeviceFailure = runtime.settings().getSettings();
        QVERIFY(afterDeviceFailure);
        QCOMPARE(afterDeviceFailure.get().audio, beforeDevice.get().audio);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
        driver->unavailableDevice.clear();
        chooseValue(*page, devices, QStringLiteral("Output B"));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(deviceContext->device()->name(), QStringLiteral("Output B"));
        chooseValue(*page, buffer, QVariant::fromValue(qint64(256)));
        if (QTest::currentTestFailed())
            return;
        chooseValue(*page, rate, 44100.0);
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(deviceContext->adoptedBufferSize(), qint64(256));
        QCOMPARE(deviceContext->adoptedSampleRate(), 44100.0);
        driver->availableDevices.append(QStringLiteral("Output C"));
        emit driver->deviceChanged();
        QTRY_VERIFY(devices->findData(QStringLiteral("Output C")) >= 0);
        QCOMPARE(devices->currentData().toString(), QStringLiteral("Output B"));
        QCOMPARE(buffer->currentData().value<qint64>(), qint64(256));
        QCOMPARE(rate->currentData().toDouble(), 44100.0);
        QVERIFY(controlPanel->isEnabled());
        page->ensureWidgetVisible(controlPanel);
        QTest::mouseClick(controlPanel, Qt::LeftButton);
        auto *selected = dynamic_cast<FixtureAudioDevice *>(deviceContext->device());
        QVERIFY(selected);
        QCOMPARE(selected->controlPanelRequests, 1);
        QCOMPARE(AudioSettings::driverName(), driver->name());
        QCOMPARE(AudioSettings::deviceName(), QStringLiteral("Output B"));
        QCOMPARE(AudioSettings::adoptedBufferSize(), qint64(256));
        QCOMPARE(AudioSettings::adoptedSampleRate(), 44100.0);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(historyManager->nextUndoEntry(), undo);
    }
    AppOptions reopened;
    QCOMPARE(reopened.audio()->obj.value("driverName").toString(), driver->name());
    QCOMPARE(reopened.audio()->obj.value("deviceName").toString(), QStringLiteral("Output B"));
    QCOMPARE(reopened.audio()->obj.value("adoptedBufferSize").toInteger(), qint64(256));
    QCOMPARE(reopened.audio()->obj.value("adoptedSampleRate").toDouble(), 44100.0);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
}

void ApplicationGuiTests::audioSettingsSaveFailureRestoresRuntimeAndAllowsRetry() {
    auto &runtime = *context->m_coreRuntime;
    const auto original = runtime.settings().getSettings();
    QVERIFY(original);
    auto *output = AudioSystem::outputSystem()->outputContext();
    auto *mixer = output->controlMixer();
    const auto gain = mixer->gain();
    const auto pan = mixer->pan();
    const auto mode = output->hotPlugNotificationMode();
    const auto restore = qScopeGuard([&] {
        mixer->setGain(gain);
        mixer->setPan(pan);
        output->setHotPlugNotificationMode(mode);
        QVERIFY(runtime.settings().updateAudio({}, original.get().audio));
    });
    const auto config = appOptions->configPath();
    TestSupport::FileWriteBlocker writeFailure(config);
    QVERIFY(writeFailure.block());
    const auto before = runtime.documentVersion();
    const auto *undo = historyManager->nextUndoEntry();
    Automation::AudioDeviceSettingsPatchDto patch;
    patch.gain = gain == 0.375f ? 0.625 : 0.375;
    patch.pan = pan == -0.25f ? 0.25 : -0.25;
    patch.hotPlugNotificationMode = mode == talcs::OutputContext::None ? talcs::OutputContext::Omni
                                                                       : talcs::OutputContext::None;
    const auto failed = runtime.settings().updateAudioDevice({}, patch);
    QVERIFY(!failed);
    QCOMPARE(failed.getError().code, Automation::AutomationErrorCode::HostCapabilityUnavailable);
    const auto rolledBack = runtime.settings().getSettings();
    QVERIFY(rolledBack);
    QCOMPARE(rolledBack.get().audio, original.get().audio);
    QCOMPARE(mixer->gain(), gain);
    QCOMPARE(mixer->pan(), pan);
    QCOMPARE(output->hotPlugNotificationMode(), mode);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
    QVERIFY(QDir(config).isEmpty());
    QVERIFY(writeFailure.restore());

    const auto retried = runtime.settings().updateAudioDevice({}, patch);
    QVERIFY2(retried, qPrintable(retried ? QString() : retried.getError().message));
    QVERIFY(retried.get().changed);
    QCOMPARE(mixer->gain(), static_cast<float>(*patch.gain));
    QCOMPARE(mixer->pan(), static_cast<float>(*patch.pan));
    QCOMPARE(
        output->hotPlugNotificationMode(),
        static_cast<talcs::OutputContext::HotPlugNotificationMode>(*patch.hotPlugNotificationMode));
    AppOptions stored;
    QCOMPARE(stored.audio()->obj.value("deviceGain").toDouble(), *patch.gain);
    QCOMPARE(stored.audio()->obj.value("devicePan").toDouble(), *patch.pan);
    QCOMPARE(stored.audio()->obj.value("hotPlugNotificationMode").toInt(),
             *patch.hotPlugNotificationMode);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(historyManager->nextUndoEntry(), undo);
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
        openOptionsPage(panel, AppOptionsGlobal::Audio);
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
        page->ensureWidgetVisible(gainSlider);
        gainSlider->setFocus();
        QTRY_VERIFY(gainSlider->hasFocus());
        QTest::keyClick(gainSlider, Qt::Key_End);
        QTRY_COMPARE(gain->value(), 6.0);
        QVERIFY(std::abs(gainSlider->displayValue() - 6.0) < 1e-4);
        QVERIFY(std::abs(mixer->gain() - 1.9952623f) < 1e-6f);
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(historyManager->nextUndoEntry(), beforeUndo);
        enterNumber(*page, gain, QLocale().toString(-6.0));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(gain->value(), -6.0);
        QVERIFY(std::abs(mixer->gain() - 0.50118723f) < 1e-6f);
        enterNumber(*page, pan, QStringLiteral("25"));
        if (QTest::currentTestFailed())
            return;
        QCOMPARE(panSlider->value(), 25.0);
        QCOMPARE(mixer->pan(), 0.25f);
        {
            auto *theme = ThemeManager::instance();
            const auto originalTheme = theme->currentThemeId();
            const auto restoreTheme =
                qScopeGuard([&] { QVERIFY(theme->applyTheme(originalTheme)); });
            for (const auto &id : {QStringLiteral("lite-light"), QStringLiteral("lite-dark")}) {
                QVERIFY(theme->applyTheme(id));
                // QSS serializes theme colors to 8-bit RGBA.
                QTRY_COMPARE(gainSlider->property("trackInactiveColor").value<QColor>().rgba(),
                             theme->semanticColor(QStringLiteral("slider.track.inactive")).rgba());
                QCOMPARE(gain->value(), -6.0);
                QCOMPARE(panSlider->value(), 25.0);
                QCOMPARE(mixer->pan(), 0.25f);
                QCOMPARE(runtime.documentVersion(), before);
            }
        }
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
        openOptionsPage(reopened, AppOptionsGlobal::Audio);
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
        openOptionsPage(panel, AppOptionsGlobal::Midi);
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
        openOptionsPage(reopened, AppOptionsGlobal::Midi);
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
