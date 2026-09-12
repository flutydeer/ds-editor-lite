#include "tst_native_desktop.h"
#include "../TestSupport/GuiAppFixture.h"
#include "../TestSupport/WaveFixture.h"

#include "Automation/CoreRuntime.h"
#include "Automation/AppOptionsAutomationAdapter.h"
#include "Controller/PlaybackController.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioSettings.h"
#include "Modules/Audio/subsystem/MidiSystem.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <TalcsCore/AudioBuffer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/TransportAudioSource.h>
#include <TalcsDevice/AudioDriver.h>
#include <TalcsDevice/AudioDriverManager.h>
#include <TalcsMidi/MidiInputDevice.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiMessageListener.h>
#include <rtmidi/RtMidi.h>

#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtTest/QTest>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <vector>

namespace {
    void runIsolatedDesktopCase() {
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("DSEL_TEST_GUI_LIFECYCLE"), QStringLiteral("1"));
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::MergedChannels);
        auto testCase = QString::fromLatin1(QTest::currentTestFunction());
        if (const auto *tag = QTest::currentDataTag(); tag && *tag)
            testCase += ':' + QString::fromLatin1(tag);
        child.start(QCoreApplication::applicationFilePath(), {testCase, QStringLiteral("-v1")});
        QVERIFY2(child.waitForStarted(), qPrintable(child.errorString()));
        const auto completed = child.waitForFinished(20000);
        const auto output = child.readAll();
        QVERIFY2(completed, output.constData());
        QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0,
                 qPrintable(QStringLiteral("Child exit code %1:\n%2")
                                .arg(child.exitCode())
                                .arg(QString::fromUtf8(output))));
    }

    class MidiReceipt final : public talcs::MidiMessageListener {
    public:
        std::atomic_int noteOnCount = 0;
        std::atomic_int noteOffCount = 0;

    protected:
        bool processMessage(const talcs::MidiMessage &message) override {
            if (message.isNoteOnOrOff() && message.getChannel() == 16 &&
                message.getNoteNumber() == 69) {
                if (message.isNoteOn())
                    ++noteOnCount;
                if (message.isNoteOff())
                    ++noteOffCount;
            }
            return false;
        }
    };
}

void NativeDesktopTests::failedAudioDriverSelectionClearsTheReleasedDevice() {
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        if (!AudioSystem::outputSystem()->outputContext()->device())
            QSKIP("No initialized output device is available for backend replacement");
        runIsolatedDesktopCase();
        return;
    }
    talcs::OutputContext output;
    QVERIFY(output.initialize(qEnvironmentVariable("DSEL_TEST_AUDIO_DRIVER"),
                              qEnvironmentVariable("DSEL_TEST_AUDIO_DEVICE")));
    const auto driverName = output.driver()->name();
    QPointer<talcs::AudioDevice> previous = output.device();
    QVERIFY(previous && previous->isOpen());
    QVERIFY(!output.setDriver(QStringLiteral("unavailable-test-backend")));
    QVERIFY(previous.isNull());
    QVERIFY(!output.driver());
    QVERIFY(output.device() == nullptr);
    QVERIFY(output.setDriver(driverName));
    QVERIFY(output.device() && output.device()->isOpen());
}

void NativeDesktopTests::audioSettingsRollbackWithoutAnInitializedBackend() {
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        runIsolatedDesktopCase();
        return;
    }
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("DSEL_TEST_DATA_ROOT", directory.path().toUtf8());
    AppOptions options;
    AudioSystem audio;
    auto *output = AudioSystem::outputSystem()->outputContext();
    QVERIFY(!output->driver() && !output->device());
    auto *mixer = output->controlMixer();
    const auto gain = mixer->gain();
    const auto pan = mixer->pan();
    const auto mode = output->hotPlugNotificationMode();
    auto services = Automation::createAppOptionsAutomationServices(&options);
    const auto original = services.snapshot().audio;
    const auto config = options.configPath();
    const auto backup = config + QStringLiteral(".backup");
    QVERIFY(QFile::rename(config, backup));
    const auto restoreFile = qScopeGuard([&] {
        if (QFile::exists(backup)) {
            if (QFileInfo(config).isDir())
                QVERIFY(QDir().rmdir(config));
            QVERIFY(QFile::rename(backup, config));
        }
    });
    QVERIFY(QDir().mkdir(config));
    auto target = original;
    target.deviceGain = 0.375;
    target.devicePan = -0.25;
    target.hotPlugNotificationMode = mode == talcs::OutputContext::None
                                         ? talcs::OutputContext::Omni
                                         : talcs::OutputContext::None;
    Automation::AudioDeviceSettingsPatchDto patch;
    patch.gain = target.deviceGain;
    patch.pan = target.devicePan;
    patch.hotPlugNotificationMode = target.hotPlugNotificationMode;
    const auto failed = services.applyAudioDevice(target, patch);
    QVERIFY(!failed);
    QCOMPARE(services.snapshot().audio, original);
    QCOMPARE(mixer->gain(), gain);
    QCOMPARE(mixer->pan(), pan);
    QCOMPARE(output->hotPlugNotificationMode(), mode);
    QVERIFY(!output->driver() && !output->device());
    QVERIFY(QDir().rmdir(config));
    QVERIFY(QFile::rename(backup, config));
    QVERIFY(services.applyAudioDevice(target, patch));
    QCOMPARE(mixer->gain(), static_cast<float>(target.deviceGain));
    QCOMPARE(mixer->pan(), static_cast<float>(target.devicePan));
    AppOptions stored;
    QCOMPARE(stored.audio()->obj.value("deviceGain").toDouble(), target.deviceGain);
    QCOMPARE(stored.audio()->obj.value("devicePan").toDouble(), target.devicePan);
}

void NativeDesktopTests::availableAudioDeviceRunsPublicPlayback() {
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(false), qPrintable(fixture.error));
    auto *output = AudioSystem::outputSystem();
    auto *deviceContext = output->outputContext();
    const auto requestedDriver = qEnvironmentVariable("DSEL_TEST_AUDIO_DRIVER");
    const auto requestedDevice = qEnvironmentVariable("DSEL_TEST_AUDIO_DEVICE");
    if (!requestedDriver.isEmpty()) {
        QVERIFY2(
            deviceContext->driverManager()->drivers().contains(requestedDriver),
            qPrintable(
                QStringLiteral("Configured audio driver is unavailable: %1").arg(requestedDriver)));
        QVERIFY2(output->setDriver(requestedDriver), qPrintable(requestedDriver));
    }
    auto *driver = deviceContext->driver();
    if (!requestedDevice.isEmpty()) {
        QVERIFY2(driver, "The configured audio device has no initialized driver");
        QVERIFY2(driver->devices().contains(requestedDevice), qPrintable(requestedDevice));
        QVERIFY2(output->setDevice(requestedDevice), qPrintable(requestedDevice));
    }
    if (requestedDriver.isEmpty() && requestedDevice.isEmpty()) {
        if (!driver)
            QSKIP("No native audio output backend is available");
        if (!deviceContext->device() && driver->devices().isEmpty())
            QSKIP("No audio output devices are enumerated");
        if (driver->name() == QStringLiteral("dummy") || driver->name() == QStringLiteral("disk"))
            QSKIP("The available output backend does not provide a playback device");
    }
    QVERIFY2(deviceContext->device(), "An enumerated output device could not be initialized");
    QVERIFY2(deviceContext->device()->isOpen(), qPrintable(deviceContext->device()->errorString()));
    const auto originalBufferSize = deviceContext->adoptedBufferSize();
    const auto originalSampleRate = deviceContext->adoptedSampleRate();
    auto &runtime = *fixture.context->m_coreRuntime;
    const auto command = [&] {
        return Automation::CommandContext{.expected = runtime.documentVersion(),
                                          .source = Automation::InvocationSource::PublicJsonRpc};
    };
    const auto restore = qScopeGuard([&] {
        runtime.playback().stop(command());
        output->setAdoptedBufferSize(originalBufferSize);
        output->setAdoptedSampleRate(originalSampleRate);
    });

    // Reopen the selected device through the production configuration path.
    const auto selectedName = deviceContext->device()->name();
    QVERIFY2(output->setDevice(selectedName), qPrintable(selectedName));
    const auto sizes = deviceContext->device()->availableBufferSizes();
    const auto alternative = std::find_if(sizes.cbegin(), sizes.cend(), [&](qint64 size) {
        return size > 0 && size <= 8192 && size != originalBufferSize;
    });
    const auto selectedBufferSize = alternative == sizes.cend() ? originalBufferSize : *alternative;
    QVERIFY(output->setAdoptedBufferSize(selectedBufferSize));
    QCOMPARE(deviceContext->device()->bufferSize(), selectedBufferSize);
    QCOMPARE(AudioSettings::adoptedBufferSize(), selectedBufferSize);
    AppOptions reopened;
    QCOMPARE(reopened.audio()->obj.value(QStringLiteral("adoptedBufferSize")).toInteger(),
             selectedBufferSize);

    const auto path = fixture.directory.filePath(QStringLiteral("silence.wav"));
    QVERIFY(TestSupport::writeWave(path, QVector<float>(48000, 0.0f)));
    auto document = Automation::DocumentAutomationFacade::newDocumentDraft(false);
    Automation::ClipDraftDto clip;
    clip.type = Automation::ClipDraftDto::Type::Audio;
    clip.properties.name = QStringLiteral("Silent playback fixture");
    clip.properties.length = 960;
    clip.properties.clipLen = 960;
    clip.audioPath = path;
    Automation::TrackDraftDto track;
    track.clips.append(clip);
    document.tracks.append(track);
    QVERIFY(runtime.documents().commitNewDocument(command(), document));
    auto *audioClip =
        dynamic_cast<AudioClip *>(*fixture.context->m_appModel->tracks().first()->clips().begin());
    QVERIFY(audioClip);
    QTRY_VERIFY(audioClip->audioInfo().frames > 0 && !audioClip->audioInfo().peakCache.isEmpty());
    QVERIFY(runtime.playback().setPosition(command(), 0));
    const auto before = runtime.documentVersion();
    QVERIFY(runtime.playback().play(command()));
    QVERIFY(deviceContext->device()->isStarted());
    QTRY_VERIFY_WITH_TIMEOUT(playbackController->position() > 0, 5000);
    auto playing = runtime.playback().getPlayback(before.documentId);
    QVERIFY(playing);
    QCOMPARE(playing.get().state, Automation::PlaybackState::Playing);
    QVERIFY(playing.get().position > 0);
    QVERIFY(runtime.playback().pause(command()));
    QTRY_COMPARE(AudioContext::instance()->transport()->playbackStatus(),
                 talcs::TransportAudioSource::Paused);
    auto paused = runtime.playback().getPlayback(before.documentId);
    QVERIFY(paused);
    QCOMPARE(paused.get().state, Automation::PlaybackState::Paused);
    QVERIFY(runtime.playback().stop(command()));
    auto stopped = runtime.playback().getPlayback(before.documentId);
    QVERIFY(stopped);
    QCOMPARE(stopped.get().state, Automation::PlaybackState::Stopped);
    QCOMPARE(runtime.documentVersion(), before);
}

void NativeDesktopTests::audioDriverStartupCanBeCanceled_data() {
    QTest::addColumn<bool>("deliverStartup");
    QTest::addColumn<bool>("destroyDriver");
    QTest::newRow("finalize-before-startup") << false << false;
    QTest::newRow("destroy-before-startup") << false << true;
    QTest::newRow("finalize-during-startup") << true << false;
}

void NativeDesktopTests::audioDriverStartupCanBeCanceled() {
    QFETCH(bool, deliverStartup);
    QFETCH(bool, destroyDriver);
    if (!qEnvironmentVariableIsSet("DSEL_TEST_GUI_LIFECYCLE")) {
        if (!AudioSystem::outputSystem()->outputContext()->driver())
            QSKIP("No audio output backend is available");
        runIsolatedDesktopCase();
        return;
    }
    GuiAppFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    auto *driver = AudioSystem::outputSystem()->outputContext()->driver();
    QVERIFY2(driver, "The enumerated audio backend failed to initialize in the child process");
    QVERIFY(driver->isInitialized());
    if (deliverStartup)
        QCoreApplication::sendPostedEvents(driver, QEvent::MetaCall);
    if (destroyDriver) {
        QPointer<talcs::AudioDriver> observed(driver);
        fixture.context.reset();
        QVERIFY(observed.isNull());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
        QTRY_VERIFY(taskManager->tasks().isEmpty());
    } else {
        driver->finalize();
        QCoreApplication::sendPostedEvents(driver, QEvent::MetaCall);
        QVERIFY(!driver->isInitialized());
        for (auto *thread : driver->findChildren<QThread *>())
            QVERIFY(!thread->isRunning());
    }
}

void NativeDesktopTests::configuredMidiLoopbackFeedsLiveSynthesizer() {
    const auto inputName = qEnvironmentVariable("DSEL_TEST_MIDI_INPUT");
    const auto outputName = qEnvironmentVariable("DSEL_TEST_MIDI_OUTPUT");
    if (inputName.isEmpty() && outputName.isEmpty())
        QSKIP("Set DSEL_TEST_MIDI_INPUT and DSEL_TEST_MIDI_OUTPUT to a dedicated loopback route");
    QVERIFY2(!inputName.isEmpty() && !outputName.isEmpty(),
             "Both MIDI loopback port names are required");
    GuiDocumentFixture fixture;
    QVERIFY2(fixture.initialize(), qPrintable(fixture.error));
    const auto inputNames = MidiSystem::availableDevices();
    const auto inputIndex = inputNames.indexOf(inputName);
    QVERIFY2(inputIndex >= 0,
             qPrintable(QStringLiteral("Configured MIDI input is unavailable: %1").arg(inputName)));
    auto *midi = AudioSystem::midiSystem();
    QVERIFY2(midi->setDevice(inputIndex), qPrintable(inputName));
    QVERIFY(midi->device() && midi->device()->isOpen());
    QCOMPARE(midi->device()->name(), inputName);
    AppOptions reopened;
    QCOMPARE(reopened.audio()->obj.value(QStringLiteral("midiDeviceIndex")).toInt(), inputIndex);
    midi->setGenerator(talcs::NoteSynthesizer::Sine);
    midi->setAttackMsec(5);
    midi->setDecayMsec(5);
    midi->setDecayRatio(1.0);
    midi->setReleaseMsec(10);
    auto *integrator = midi->integrator();
    QVERIFY(integrator->open(256, 48000));
    MidiReceipt received;
    midi->device()->listener()->addFilter(&received);
    const auto detach = qScopeGuard([&] {
        midi->device()->listener()->removeFilter(&received);
        integrator->close();
        midi->device()->close();
    });
    try {
        RtMidiOut sender;
        unsigned int outputIndex = sender.getPortCount();
        for (unsigned int index = 0; index < sender.getPortCount(); ++index) {
            if (QString::fromStdString(sender.getPortName(index)) == outputName) {
                outputIndex = index;
                break;
            }
        }
        QVERIFY2(outputIndex < sender.getPortCount(),
                 qPrintable(
                     QStringLiteral("Configured MIDI output is unavailable: %1").arg(outputName)));
        sender.openPort(outputIndex, "DS Editor test loopback");
        QVERIFY(sender.isPortOpen());
        const std::vector<unsigned char> noteOn{0x9f, 69, 16};
        const std::vector<unsigned char> noteOff{0x8f, 69, 0};
        const auto releaseNote = qScopeGuard([&] {
            try {
                sender.sendMessage(&noteOff);
            } catch (const std::exception &error) {
                qWarning() << "MIDI loopback cleanup:" << error.what();
            }
        });
        sender.sendMessage(&noteOn);
        QTRY_COMPARE_WITH_TIMEOUT(received.noteOnCount.load(), 1, 5000);
        talcs::AudioBuffer buffer(2, 256);
        QCOMPARE(integrator->read(&buffer), qint64{256});
        bool nonzero = false;
        for (int channel = 0; channel < buffer.channelCount(); ++channel) {
            for (qint64 sample = 0; sample < buffer.sampleCount(); ++sample) {
                const auto value = buffer.constSampleAt(channel, sample);
                QVERIFY(std::isfinite(value));
                nonzero |= value != 0.0f;
            }
        }
        QVERIFY(nonzero);
        sender.sendMessage(&noteOff);
        QTRY_COMPARE_WITH_TIMEOUT(received.noteOffCount.load(), 1, 5000);
        for (int block = 0; block < 4; ++block)
            QCOMPARE(integrator->read(&buffer), qint64{256});
        for (int channel = 0; channel < buffer.channelCount(); ++channel)
            for (qint64 sample = 0; sample < buffer.sampleCount(); ++sample)
                QCOMPARE(buffer.constSampleAt(channel, sample), 0.0f);
    } catch (const std::exception &error) {
        QFAIL(error.what());
    }
}
