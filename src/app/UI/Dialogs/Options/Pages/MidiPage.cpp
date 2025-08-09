#include "MidiPage.h"

#include <cmath>

#include <QDebug>
#include <QBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/Decibels.h>
#include <TalcsMidi/MidiInputDevice.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include "Utils/Decibellinearizer.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SvsExpressionSpinBox.h"
#include "UI/Controls/SvsExpressionDoubleSpinBox.h"

#include <Modules/Audio/AudioSystem.h>

#include <Modules/Audio/subsystem/MidiSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <Modules/Audio/utils/AudioHelpers.h>
#include <Modules/Audio/utils/SettingPagesSynthHelper.h>

class MIDIPageWidget : public QWidget {
    Q_OBJECT
public:
    explicit MIDIPageWidget(QWidget *parent = nullptr)
        : QWidget(parent), d(new SettingPageSynthHelper(this)) {
        const auto mainLayout = new QVBoxLayout;

        const auto inputGroupBox = new QGroupBox(tr("MIDI Input"));
        const auto inputLayout = new QFormLayout;
        const auto deviceComboBox = new QComboBox;
        inputLayout->addRow(tr("&Device"), deviceComboBox);
        inputGroupBox->setLayout(inputLayout);
        mainLayout->addWidget(inputGroupBox);

        const auto synthesizerGroupBox = new QGroupBox(tr("Synthesizer"));
        const auto synthesizerLayout = new QFormLayout;
        const auto generatorComboBox = new QComboBox;
        generatorComboBox->addItems(
            {tr("Sine wave"), tr("Square wave"), tr("Triangle Wave"), tr("Sawtooth wave")});
        synthesizerLayout->addRow(tr("&Generator"), generatorComboBox);

        const auto amplitudeLayout = new QHBoxLayout;
        const auto amplitudeSlider = new SVS::SeekBar;
        amplitudeSlider->setDefaultValue(DecibelLinearizer::decibelToLinearValue(-3));
        amplitudeSlider->setRange(DecibelLinearizer::decibelToLinearValue(-96),
                                  DecibelLinearizer::decibelToLinearValue(0));
        amplitudeLayout->addWidget(amplitudeSlider);
        const auto amplitudeSpinBox = new SVS::ExpressionDoubleSpinBox;
        amplitudeSpinBox->setDecimals(1);
        amplitudeSpinBox->setRange(-96, 0);
        amplitudeSpinBox->setSpecialValueText("-INF");
        amplitudeLayout->addWidget(amplitudeSpinBox);
        const auto amplitudeLabel = new QLabel(tr("&Amplitude (dB)"));
        amplitudeLabel->setBuddy(amplitudeSpinBox);
        synthesizerLayout->addRow(amplitudeLabel, amplitudeLayout);

        const auto attackLayout = new QHBoxLayout;
        const auto attackSlider = new SVS::SeekBar;
        attackSlider->setInterval(1);
        attackSlider->setDefaultValue(10);
        attackSlider->setRange(0, 100);
        attackLayout->addWidget(attackSlider);
        const auto attackSpinBox = new SVS::ExpressionSpinBox;
        attackSpinBox->setRange(0, 100);
        attackLayout->addWidget(attackSpinBox);
        const auto attackLabel = new QLabel(tr("A&ttack (ms)"));
        attackLabel->setBuddy(attackSpinBox);
        synthesizerLayout->addRow(attackLabel, attackLayout);

        const auto decayLayout = new QHBoxLayout;
        const auto decaySlider = new SVS::SeekBar;
        decaySlider->setInterval(1);
        decaySlider->setDefaultValue(1000);
        decaySlider->setRange(0, 1000);
        decayLayout->addWidget(decaySlider);
        const auto decaySpinBox = new SVS::ExpressionSpinBox;
        decaySpinBox->setRange(0, 1000);
        decayLayout->addWidget(decaySpinBox);
        const auto decayLabel = new QLabel(tr("D&ecay (ms)"));
        decayLabel->setBuddy(decaySpinBox);
        synthesizerLayout->addRow(decayLabel, decayLayout);

        const auto decayRatioLayout = new QHBoxLayout;
        const auto decayRatioSlider = new SVS::SeekBar;
        decayRatioSlider->setDefaultValue(0.5);
        decayRatioSlider->setRange(0, 1);
        decayRatioLayout->addWidget(decayRatioSlider);
        const auto decayRatioSpinBox = new SVS::ExpressionDoubleSpinBox;
        decayRatioSpinBox->setRange(0, 1);
        decayRatioLayout->addWidget(decayRatioSpinBox);
        const auto decayRatioLabel = new QLabel(tr("Decay rati&o"));
        decayRatioLabel->setBuddy(decayRatioSpinBox);
        synthesizerLayout->addRow(decayRatioLabel, decayRatioLayout);

        const auto releaseLayout = new QHBoxLayout;
        const auto releaseSlider = new SVS::SeekBar;
        releaseSlider->setInterval(1);
        releaseSlider->setDefaultValue(50);
        releaseSlider->setRange(0, 100);
        releaseLayout->addWidget(releaseSlider);
        const auto releaseSpinBox = new SVS::ExpressionSpinBox;
        releaseSpinBox->setRange(0, 100);
        releaseLayout->addWidget(releaseSpinBox);
        const auto releaseLabel = new QLabel(tr("&Release (ms)"));
        releaseLabel->setBuddy(releaseSpinBox);
        synthesizerLayout->addRow(releaseLabel, releaseLayout);

        const auto frequencyOfALayout = new QVBoxLayout;
        auto frequencyOfASpinBox = new SVS::ExpressionDoubleSpinBox;
        frequencyOfASpinBox->setRange(440.0 * std::pow(2, -1.0 / 24.0),
                                      440.0 * std::pow(2, 1.0 / 24.0));
        frequencyOfALayout->addWidget(frequencyOfASpinBox);
        auto adjustByProjectCheckBox =
            new QCheckBox(tr("Ad&just by the cent shift of the active project window"));
        frequencyOfALayout->addWidget(adjustByProjectCheckBox);
        const auto frequencyOfALabel = new QLabel(tr("&Frequency of A"));
        frequencyOfALabel->setBuddy(frequencyOfASpinBox);
        synthesizerLayout->addRow(frequencyOfALabel, frequencyOfALayout);

        const auto synthesizerButtonLayout = new QHBoxLayout;
        const auto synthesizerTestButton = new QPushButton(tr("&Preview"));
        synthesizerTestButton->setCheckable(true);
        synthesizerButtonLayout->addWidget(synthesizerTestButton);
        const auto flushButton = new QPushButton(tr("&Interrupt All Notes"));
        flushButton->setToolTip(
            tr("Interrupt all notes that are currently played by the synthesizer"));
        synthesizerButtonLayout->addWidget(flushButton);
        synthesizerButtonLayout->addStretch();
        synthesizerLayout->addRow(synthesizerButtonLayout);

        synthesizerGroupBox->setLayout(synthesizerLayout);
        mainLayout->addWidget(synthesizerGroupBox);
        setLayout(mainLayout);

        const auto controlGroupBox = new QGroupBox(tr("Control"));

        mainLayout->addWidget(controlGroupBox, 1);

        auto ms = AudioSystem::midiSystem();

        const auto deviceList = talcs::MidiInputDevice::devices();
        if (!ms->device()) {
            deviceComboBox->addItem(tr("(Not working)"), -1);
        }
        for (int i = 0; i < deviceList.size(); i++) {
            deviceComboBox->addItem(deviceList.at(i), i);
            if (ms->device() && i == ms->device()->deviceIndex())
                deviceComboBox->setCurrentIndex(i);
        }
        connect(deviceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [ms, deviceComboBox, this](const int index) {
                    const auto i = deviceComboBox->itemData(index).toInt();
                    if (i == -1)
                        return;
                    if (!ms->setDevice(i)) {
                        QMessageBox::critical(
                            this, {},
                            tr("Cannot open MIDI device %1").arg(deviceComboBox->itemText(index)));
                        QSignalBlocker o(deviceComboBox);
                        deviceComboBox->setCurrentIndex(deviceComboBox->findData(
                            ms->device() ? ms->device()->deviceIndex() : -1));
                    } else {
                        QSignalBlocker o(deviceComboBox);
                        if (const auto notWorkingIndex = deviceComboBox->findData(-1);
                            notWorkingIndex != -1)
                            deviceComboBox->removeItem(notWorkingIndex);
                    }
                });

        d->m_cachedGenerator = ms->generator();
        d->m_cachedAmplitude = ms->amplitudeDecibel();
        d->m_cachedAttackMsec = ms->attackMsec();
        d->m_cachedDecayMsec = ms->decayMsec();
        d->m_cachedDecayRatio = ms->decayRatio();
        d->m_cachedReleaseMsec = ms->releaseMsec();
        d->m_cachedFrequencyOfA = ms->frequencyOfA();

        d->initialize(generatorComboBox, amplitudeSlider, amplitudeSpinBox, attackSlider,
                      attackSpinBox, decaySlider, decaySpinBox, decayRatioSlider, decayRatioSpinBox,
                      releaseSlider, releaseSpinBox, synthesizerTestButton);

        connect(frequencyOfASpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [adjustByProjectCheckBox, this](const double value) {
                    d->m_mutex.lock();
                    if (!adjustByProjectCheckBox->isChecked())
                        d->m_cachedFrequencyOfA = value;
                    d->m_mutex.unlock();
                    d->m_testSynthesizer.flush();
                });
        connect(adjustByProjectCheckBox, &QAbstractButton::clicked, this,
                [frequencyOfASpinBox, this](const bool checked) {
                    d->m_mutex.lock();
                    if (checked) {
                        d->m_cachedFrequencyOfA = 0;
                        frequencyOfASpinBox->setDisabled(true);
                    } else {
                        d->m_cachedFrequencyOfA = frequencyOfASpinBox->value();
                        frequencyOfASpinBox->setDisabled(false);
                    }
                    d->m_mutex.unlock();
                    d->m_testSynthesizer.flush();
                });

        d->m_cachedFrequencyOfA = ms->frequencyOfA();
        if (qFuzzyIsNull(d->m_cachedFrequencyOfA)) {
            adjustByProjectCheckBox->setChecked(true);
            frequencyOfASpinBox->setDisabled(true);
            frequencyOfASpinBox->setValue(440.0);
        } else {
            frequencyOfASpinBox->setValue(d->m_cachedFrequencyOfA);
        }

        connect(synthesizerTestButton, &QAbstractButton::clicked, this,
                [synthesizerTestButton, this](const bool checked) {
                    if (checked) {
                        if (!AudioSystem::outputSystem()->isReady()) {
                            synthesizerTestButton->setChecked(false);
                            return;
                        }
                    }
                    d->toggleTestState(checked);
                });
        connect(
            d, &SettingPageSynthHelper::testFinished, this,
            [synthesizerTestButton] { synthesizerTestButton->setChecked(false); },
            Qt::QueuedConnection);
        connect(flushButton, &QAbstractButton::clicked, this,
                [] { AudioSystem::midiSystem()->synthesizer()->noteSynthesizer()->flush(); });
    }

    ~MIDIPageWidget() override = default;

    void accept() const {
        const auto ms = AudioSystem::midiSystem();
        ms->setGenerator(d->m_cachedGenerator);
        ms->setAmplitudeDecibel(d->m_cachedAmplitude);
        ms->setAttackMsec(d->m_cachedAttackMsec);
        ms->setDecayMsec(d->m_cachedDecayMsec);
        ms->setDecayRatio(d->m_cachedDecayRatio);
        ms->setReleaseMsec(d->m_cachedReleaseMsec);
        ms->setFrequencyOfA(d->m_cachedFrequencyOfA);
    }

    SettingPageSynthHelper *d;
};

MidiPage::MidiPage(QWidget *parent) : IOptionPage(parent) {
    const auto mainLayout = new QVBoxLayout;

    m_widget = new MIDIPageWidget;

    mainLayout->addWidget(m_widget);
    setLayout(mainLayout);
}

MidiPage::~MidiPage() {
    MidiPage::modifyOption();
}

void MidiPage::modifyOption() {
    m_widget->accept();
}

#include "MidiPage.moc"
