#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/subsystem/OutputSystem.h"
#include "Modules/Audio/utils/SettingPagesSynthHelper.h"

#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/History/HistoryManager.h>
#include <TalcsCore/AudioBuffer.h>
#include <TalcsDevice/AudioDevice.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include <QComboBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

#include <cmath>

void ApplicationGuiTests::settingsSynthPreviewKeepsEnvelopeDurationsAcrossSampleRates() {
    auto *output = AudioSystem::outputSystem()->context();
    QVERIFY(!output->device() || !output->device()->isStarted());
    auto *mixer = output->preMixer();
    auto *midiSynth = AudioSystem::midiSystem()->synthesizer()->noteSynthesizer();
    const auto previousMidiConfig = midiSynth->config();
    const bool wasOpen = mixer->isOpen();
    const auto previousBufferSize = mixer->bufferSize();
    const auto previousSampleRate = mixer->sampleRate();
    const auto restoreOutput = qScopeGuard([&] {
        bool restored = true;
        if (wasOpen) {
            restored = mixer->open(previousBufferSize, previousSampleRate);
            if (restored)
                emit output->sampleRateChanged(previousSampleRate);
        } else {
            mixer->close();
        }
        midiSynth->setConfig(previousMidiConfig);
        QVERIFY2(restored, "The fixture must restore the original output mixer configuration");
    });
    constexpr qint64 blockSize = 256;
    QVERIFY(mixer->open(blockSize, 48000));
    const auto originalSources = mixer->sources();
    const auto before = context->m_coreRuntime->documentVersion();
    QComboBox generator;
    generator.addItems({"Sine", "Square", "Triangle", "Sawtooth"});
    SVS::SeekBar amplitudeSlider;
    SVS::ExpressionDoubleSpinBox amplitude;
    amplitude.setRange(-96, 12);
    SVS::SeekBar attackSlider;
    SVS::ExpressionSpinBox attack;
    SVS::SeekBar decaySlider;
    SVS::ExpressionSpinBox decay;
    SVS::SeekBar sustainSlider;
    SVS::ExpressionDoubleSpinBox sustain;
    sustain.setRange(0, 1);
    sustainSlider.setRange(0, 1);
    SVS::SeekBar releaseSlider;
    SVS::ExpressionSpinBox release;
    QPushButton preview;
    {
        SettingPageSynthHelper helper;
        helper.m_cachedGenerator = talcs::NoteSynthesizer::Sine;
        helper.m_cachedAmplitude = -12;
        helper.m_cachedAttackMsec = 10;
        helper.m_cachedDecayMsec = 30;
        helper.m_cachedDecayRatio = 0.5;
        helper.m_cachedReleaseMsec = 20;
        helper.initialize(&generator, &amplitudeSlider, &amplitude, &attackSlider, &attack,
                          &decaySlider, &decay, &sustainSlider, &sustain, &releaseSlider, &release,
                          &preview);
        QVERIFY(mixer->sources().contains(&helper.m_testMixer));
        QSignalSpy finished(&helper, &SettingPageSynthHelper::testFinished);
        talcs::AudioBuffer buffer(2, blockSize);
        const auto stopOnFailure = qScopeGuard([&] {
            helper.toggleTestState(false);
            if (helper.m_testMixer.isOpen()) {
                helper.m_testMixer.read(&buffer);
                helper.m_testMixer.read(&buffer);
            }
            helper.m_testSynthesizer.flush(true);
        });
        for (const auto rate : {48000.0, 96000.0}) {
            QVERIFY(mixer->open(blockSize, rate));
            // This is the notification emitted after the output graph adopts a new rate.
            emit output->sampleRateChanged(rate);
            QCOMPARE(helper.m_testMixer.sampleRate(), rate);
            QCOMPARE(helper.m_testSynthesizer.sampleRate(), rate);
            QCOMPARE(helper.m_testSynthesizer.attackTime(), qint64(rate * 0.010));
            QCOMPARE(helper.m_testSynthesizer.decayTime(), qint64(rate * 0.030));
            QCOMPARE(helper.m_testSynthesizer.releaseTime(), qint64(rate * 0.020));
            QCOMPARE(helper.m_testSynthesizer.decayRatio(), 0.5);
            const auto previousFinished = finished.count();
            helper.toggleTestState(true);
            QCOMPARE(helper.m_testMixer.read(&buffer), blockSize);
            bool hasSignal = false;
            for (int channel = 0; channel < buffer.channelCount(); ++channel) {
                for (qint64 sample = 0; sample < buffer.sampleCount(); ++sample) {
                    const auto value = buffer.constSampleAt(channel, sample);
                    QVERIFY(std::isfinite(value));
                    hasSignal |= value != 0.0f;
                }
            }
            QVERIFY(hasSignal);
            QCOMPARE(finished.count(), previousFinished);
            helper.toggleTestState(false);
            const auto releaseBlocks =
                (helper.m_testSynthesizer.releaseTime() + blockSize - 1) / blockSize + 2;
            for (qint64 block = 0; block < releaseBlocks; ++block)
                QCOMPARE(helper.m_testMixer.read(&buffer), blockSize);
            QCOMPARE(finished.count(), previousFinished + 1);
            QVERIFY(helper.isTestFinished);
            for (int channel = 0; channel < buffer.channelCount(); ++channel) {
                for (qint64 sample = 0; sample < buffer.sampleCount(); ++sample)
                    QCOMPARE(buffer.constSampleAt(channel, sample), 0.0f);
            }
        }
    }
    QCOMPARE(mixer->sources(), originalSources);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
