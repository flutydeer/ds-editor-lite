#ifndef SETTINGPAGESSYNTHHELPER_H
#define SETTINGPAGESSYNTHHELPER_H

#include <QMutex>

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

namespace SVS {
    class SeekBar;
    class ExpressionSpinBox;
    class ExpressionDoubleSpinBox;
}

class QComboBox;

class SettingPageSynthHelper : public QObject, public talcs::NoteSynthesizerDetector {
    Q_OBJECT
public:
    explicit SettingPageSynthHelper(QObject *parent = nullptr);
    ~SettingPageSynthHelper() override;
    void initialize(QComboBox *generatorComboBox, SVS::SeekBar *amplitudeSlider,
                    SVS::ExpressionDoubleSpinBox *amplitudeSpinBox, SVS::SeekBar *attackSlider,
                    SVS::ExpressionSpinBox *attackSpinBox, SVS::SeekBar *decaySlider,
                    SVS::ExpressionSpinBox *decaySpinBox, SVS::SeekBar *decayRatioSlider,
                    SVS::ExpressionDoubleSpinBox *decayRatioSpinBox, SVS::SeekBar *releaseSlider,
                    SVS::ExpressionSpinBox *releaseSpinBox, QWidget *previewButton = nullptr);

    void detectInterval(qint64 intervalLength) override;
    talcs::NoteSynthesizerDetectorMessage nextMessage() override;

    void toggleTestState(bool enabled);

    /// Plays a note through the live preview chain, which follows the unsaved
    /// control values of the hosting settings page.
    void noteOn(int key);
    void noteOff(int key);
    /// Drops queued preview notes and cuts the ones already sounding.
    void flushPreviewNotes();

    talcs::MixerAudioSource m_testMixer;
    talcs::NoteSynthesizer m_testSynthesizer;

    // Live preview chain of the settings pages' keyboards. It needs its own
    // NoteSynthesizer because MidiNoteSynthesizer takes over the detector of the
    // synthesizer it wraps, which would silence the preview score above.
    talcs::MidiNoteSynthesizer m_previewMidiSynthesizer;
    talcs::MidiMessageIntegrator m_previewMidiIntegrator;

    QMutex m_mutex;
    double m_currentPosition = 0;
    double m_currentInterval = 0;
    int m_currentScoreIndex = -1;
    QAtomicInteger<bool> isTestFinished = true;

    const talcs::NoteSynthesizerDetectorMessage *m_adoptedScores;

    int m_cachedGenerator;
    double m_cachedAmplitude;
    int m_cachedAttackMsec;
    int m_cachedDecayMsec;
    double m_cachedDecayRatio;
    int m_cachedReleaseMsec;
    double m_cachedFrequencyOfA = 0.0;

signals:
    void testFinished();

private:
    void syncPreviewConfig();
};

#endif // SETTINGPAGESSYNTHHELPER_H
