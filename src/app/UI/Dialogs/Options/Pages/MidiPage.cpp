#include "MidiPage.h"

#include <cmath>

#include <QDebug>
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/Decibels.h>
#include <TalcsMidi/MidiInputDevice.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include "Utils/Decibellinearizer.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SvsExpressionSpinBox.h"
#include "UI/Controls/SvsExpressionDoubleSpinBox.h"
#include "UI/Controls/OptionListCard.h"
#include "UI/Controls/SwitchButton.h"

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
        mainLayout->setContentsMargins({});

        const auto deviceComboBox = new QComboBox;
        const auto inputCard = new OptionListCard(tr("MIDI Input"));
        inputCard->addItem(tr("&Device"), deviceComboBox);
        mainLayout->addWidget(inputCard);

        const auto generatorComboBox = new QComboBox;
        generatorComboBox->addItems(
            {tr("Sine wave"), tr("Square wave"), tr("Triangle Wave"), tr("Sawtooth wave")});

        m_amplitudeSlider = new SVS::SeekBar;
        m_amplitudeSlider->setFixedWidth(256);
        m_amplitudeSlider->setDefaultValue(DecibelLinearizer::decibelToLinearValue(-3));
        m_amplitudeSlider->setRange(DecibelLinearizer::decibelToLinearValue(-96),
                                    DecibelLinearizer::decibelToLinearValue(0));
        m_amplitudeSpinBox = new SVS::ExpressionDoubleSpinBox;
        m_amplitudeSpinBox->setDecimals(1);
        m_amplitudeSpinBox->setRange(-96, 0);
        m_amplitudeSpinBox->setSpecialValueText("-INF");

        m_attackSlider = new SVS::SeekBar;
        m_attackSlider->setFixedWidth(256);
        m_attackSlider->setInterval(1);
        m_attackSlider->setDefaultValue(10);
        m_attackSlider->setRange(0, 100);
        m_attackSpinBox = new SVS::ExpressionSpinBox;
        m_attackSpinBox->setRange(0, 100);

        m_decaySlider = new SVS::SeekBar;
        m_decaySlider->setFixedWidth(256);
        m_decaySlider->setInterval(1);
        m_decaySlider->setDefaultValue(1000);
        m_decaySlider->setRange(0, 1000);
        m_decaySpinBox = new SVS::ExpressionSpinBox;
        m_decaySpinBox->setRange(0, 1000);

        m_decayRatioSlider = new SVS::SeekBar;
        m_decayRatioSlider->setFixedWidth(256);
        m_decayRatioSlider->setDefaultValue(0.5);
        m_decayRatioSlider->setRange(0, 1);
        m_decayRatioSpinBox = new SVS::ExpressionDoubleSpinBox;
        m_decayRatioSpinBox->setRange(0, 1);

        m_releaseSlider = new SVS::SeekBar;
        m_releaseSlider->setFixedWidth(256);
        m_releaseSlider->setInterval(1);
        m_releaseSlider->setDefaultValue(50);
        m_releaseSlider->setRange(0, 100);
        m_releaseSpinBox = new SVS::ExpressionSpinBox;
        m_releaseSpinBox->setRange(0, 100);

        m_frequencyOfASpinBox = new SVS::ExpressionDoubleSpinBox;
        m_frequencyOfASpinBox->setRange(440.0 * std::pow(2, -1.0 / 24.0),
                                        440.0 * std::pow(2, 1.0 / 24.0));
        m_adjustByProjectSwitch = new SwitchButton;

        const auto synthesizerTestButton = new QPushButton(tr("&Preview"));
        synthesizerTestButton->setCheckable(true);
        const auto flushButton = new QPushButton(tr("&Interrupt All Notes"));
        flushButton->setToolTip(
            tr("Interrupt all notes that are currently played by the synthesizer"));

        const auto synthesizerCard = new OptionListCard(tr("Synthesizer"));
        synthesizerCard->addItem(tr("&Generator"), generatorComboBox);
        synthesizerCard->addItem(tr("&Amplitude (dB)"),
                                 {m_amplitudeSlider, m_amplitudeSpinBox});
        synthesizerCard->addItem(tr("A&ttack (ms)"), {m_attackSlider, m_attackSpinBox});
        synthesizerCard->addItem(tr("D&ecay (ms)"), {m_decaySlider, m_decaySpinBox});
        synthesizerCard->addItem(tr("Decay rati&o"), {m_decayRatioSlider, m_decayRatioSpinBox});
        synthesizerCard->addItem(tr("&Release (ms)"), {m_releaseSlider, m_releaseSpinBox});
        synthesizerCard->addItem(tr("&Frequency of A"), m_frequencyOfASpinBox);
        synthesizerCard->addItem(tr("Adjust by project cent shift"), m_adjustByProjectSwitch);
        synthesizerCard->addItem(tr("Control"), {synthesizerTestButton, flushButton});
        mainLayout->addWidget(synthesizerCard);

        mainLayout->addStretch();
        setLayout(mainLayout);

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

        d->initialize(generatorComboBox, m_amplitudeSlider, m_amplitudeSpinBox, m_attackSlider,
                      m_attackSpinBox, m_decaySlider, m_decaySpinBox, m_decayRatioSlider, m_decayRatioSpinBox,
                      m_releaseSlider, m_releaseSpinBox, synthesizerTestButton);

        connect(m_frequencyOfASpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](const double value) {
                    d->m_mutex.lock();
                    if (!m_adjustByProjectSwitch->value())
                        d->m_cachedFrequencyOfA = value;
                    d->m_mutex.unlock();
                    d->m_testSynthesizer.flush();
                });
        connect(m_adjustByProjectSwitch, &SwitchButton::toggled, this,
                [this](const bool checked) {
                    d->m_mutex.lock();
                    if (checked) {
                        d->m_cachedFrequencyOfA = 0;
                        m_frequencyOfASpinBox->setDisabled(true);
                    } else {
                        d->m_cachedFrequencyOfA = m_frequencyOfASpinBox->value();
                        m_frequencyOfASpinBox->setDisabled(false);
                    }
                    d->m_mutex.unlock();
                    d->m_testSynthesizer.flush();
                });

        d->m_cachedFrequencyOfA = ms->frequencyOfA();
        if (qFuzzyIsNull(d->m_cachedFrequencyOfA)) {
            m_adjustByProjectSwitch->setValue(true);
            m_frequencyOfASpinBox->setDisabled(true);
            m_frequencyOfASpinBox->setValue(440.0);
        } else {
            m_frequencyOfASpinBox->setValue(d->m_cachedFrequencyOfA);
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
    SVS::SeekBar *m_amplitudeSlider = nullptr;
    SVS::ExpressionDoubleSpinBox *m_amplitudeSpinBox = nullptr;
    SVS::SeekBar *m_attackSlider = nullptr;
    SVS::ExpressionSpinBox *m_attackSpinBox = nullptr;
    SVS::SeekBar *m_decaySlider = nullptr;
    SVS::ExpressionSpinBox *m_decaySpinBox = nullptr;
    SVS::SeekBar *m_decayRatioSlider = nullptr;
    SVS::ExpressionDoubleSpinBox *m_decayRatioSpinBox = nullptr;
    SVS::SeekBar *m_releaseSlider = nullptr;
    SVS::ExpressionSpinBox *m_releaseSpinBox = nullptr;
    SVS::ExpressionDoubleSpinBox *m_frequencyOfASpinBox = nullptr;
    SwitchButton *m_adjustByProjectSwitch = nullptr;
};

MidiPage::MidiPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

MidiPage::~MidiPage() {
    MidiPage::modifyOption();
}

void MidiPage::modifyOption() {
    m_widget->accept();
}

QWidget * MidiPage::createContentWidget() {
    m_widget = new MIDIPageWidget;
    m_widget->setContentsMargins({});
    return m_widget;
}

#include "MidiPage.moc"
