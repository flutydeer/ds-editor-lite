#ifndef SETTINGPAGESSYNTHHELPER_H
#define SETTINGPAGESSYNTHHELPER_H

#include <QMutex>

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>

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
                    SVS::ExpressionDoubleSpinBox *decayRatioSpinBox,
                    SVS::SeekBar *releaseSlider, SVS::ExpressionSpinBox *releaseSpinBox, QWidget *previewButton);

    void detectInterval(qint64 intervalLength) override;
    talcs::NoteSynthesizerDetectorMessage nextMessage() override;

    void toggleTestState(bool enabled);

    talcs::MixerAudioSource m_testMixer;
    talcs::NoteSynthesizer m_testSynthesizer;

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

};



#endif // SETTINGPAGESSYNTHHELPER_H
