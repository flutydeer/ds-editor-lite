#include "PseudoSingerPage.h"

#include "Modules/Audio/utils/PseudoSingerConfigNotifier.h"

#include <QBoxLayout>
#include <QGroupBox>
#include <QApplication>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>

#include "Utils/Decibellinearizer.h"
#include "UI/Controls/SvsSeekbar.h"
#include "UI/Controls/SvsExpressionSpinBox.h"
#include "UI/Controls/SvsExpressionDoubleSpinBox.h"

#include <Modules/Audio/utils/SettingPagesSynthHelper.h>
#include <Modules/Audio/AudioSettings.h>
#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>

class PseudoSingerPageWidget : public QWidget {
    Q_OBJECT
public:
    explicit PseudoSingerPageWidget(QWidget *parent = nullptr)
        : QWidget(parent), d(new SettingPageSynthHelper) {
        const auto mainLayout = new QVBoxLayout;

        const auto synthesizerGroupBox = new QGroupBox(tr("Synthesizer"));
        const auto synthesizerMainLayout = new QVBoxLayout;

        const auto synthesizerHeaderLayout = new QFormLayout;
        const auto synthesizerSelectLayout = new QHBoxLayout;
        const auto synthesizerSelectComboBox = new QComboBox;
        synthesizerSelectComboBox->addItems({"Synth 1", "Synth 2", "Synth 3", "Synth 4"});
        synthesizerSelectLayout->addWidget(synthesizerSelectComboBox, 1);
        const auto synthesizerTestButton = new QPushButton(tr("&Preview"));
        synthesizerTestButton->setCheckable(true);
        synthesizerSelectLayout->addWidget(synthesizerTestButton);
        const auto synthesizerSelectLabel = new QLabel(tr("&Synth No."));
        synthesizerSelectLabel->setBuddy(synthesizerSelectComboBox);
        synthesizerHeaderLayout->addRow(synthesizerSelectLabel, synthesizerSelectLayout);
        synthesizerMainLayout->addLayout(synthesizerHeaderLayout);

        const auto synthesizerBodyLayout = new QHBoxLayout;
        synthesizerBodyLayout->addSpacing(32);
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
        attackSlider->setDefaultValue(50);
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

        synthesizerBodyLayout->addLayout(synthesizerLayout);
        synthesizerMainLayout->addLayout(synthesizerBodyLayout);
        synthesizerGroupBox->setLayout(synthesizerMainLayout);
        mainLayout->addWidget(synthesizerGroupBox);

        const auto parameterGroupBox = new QGroupBox(tr("Parameter"));
        const auto parameterLayout = new QVBoxLayout;
        const auto readPitchCheckBox = new QCheckBox(tr("Read pitc&h"));
        parameterLayout->addWidget(readPitchCheckBox);
        const auto readEnergyCheckBox = new QCheckBox(tr("Read energ&y"));
        parameterLayout->addWidget(readEnergyCheckBox);
        parameterGroupBox->setLayout(parameterLayout);
        mainLayout->addWidget(parameterGroupBox);

        mainLayout->addStretch();

        setLayout(mainLayout);

        for (int i = 0; i < 4; i++) {
            m_cachedSynthConfig[i].generator = AudioSettings::pseudoSingerSynthGenerator(i);
            m_cachedSynthConfig[i].amplitude = AudioSettings::pseudoSingerSynthAmplitude(i);
            m_cachedSynthConfig[i].attackMsec = AudioSettings::pseudoSingerSynthAttackMsec(i);
            m_cachedSynthConfig[i].decayMsec = AudioSettings::pseudoSingerSynthDecayMsec(i);
            m_cachedSynthConfig[i].decayRatio = AudioSettings::pseudoSingerSynthDecayRatio(i);
            m_cachedSynthConfig[i].releaseMsec = AudioSettings::pseudoSingerSynthReleaseMsec(i);
        }

        d->m_cachedGenerator = m_cachedSynthConfig[0].generator;
        d->m_cachedAmplitude = m_cachedSynthConfig[0].amplitude;
        d->m_cachedAttackMsec = m_cachedSynthConfig[0].attackMsec;
        d->m_cachedDecayMsec = m_cachedSynthConfig[0].decayMsec;
        d->m_cachedDecayRatio = m_cachedSynthConfig[0].decayRatio;
        d->m_cachedReleaseMsec = m_cachedSynthConfig[0].releaseMsec;

        static double amplitudeDefaultValue[] = {DecibelLinearizer::decibelToLinearValue(-4.0),
                                                 DecibelLinearizer::decibelToLinearValue(-7.0),
                                                 DecibelLinearizer::decibelToLinearValue(-4.0),
                                                 DecibelLinearizer::decibelToLinearValue(-6.0)};
        amplitudeSlider->setDefaultValue(amplitudeDefaultValue[d->m_cachedGenerator]);

        d->initialize(generatorComboBox, amplitudeSlider, amplitudeSpinBox, attackSlider,
                      attackSpinBox, decaySlider, decaySpinBox, decayRatioSlider, decayRatioSpinBox,
                      releaseSlider, releaseSpinBox, synthesizerTestButton);

        connect(synthesizerTestButton, &QAbstractButton::clicked, this,
                [=, this](const bool checked) {
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
            [=] { synthesizerTestButton->setChecked(false); }, Qt::QueuedConnection);

        connect(synthesizerSelectComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [=, this](const int index) {
                    m_cachedSynthConfig[m_oldSynthIndex].generator = d->m_cachedGenerator;
                    m_cachedSynthConfig[m_oldSynthIndex].amplitude = d->m_cachedAmplitude;
                    m_cachedSynthConfig[m_oldSynthIndex].attackMsec = d->m_cachedAttackMsec;
                    m_cachedSynthConfig[m_oldSynthIndex].decayMsec = d->m_cachedDecayMsec;
                    m_cachedSynthConfig[m_oldSynthIndex].decayRatio = d->m_cachedDecayRatio;
                    m_cachedSynthConfig[m_oldSynthIndex].releaseMsec = d->m_cachedReleaseMsec;

                    generatorComboBox->setCurrentIndex(m_cachedSynthConfig[index].generator);
                    amplitudeSpinBox->setValue(m_cachedSynthConfig[index].amplitude);
                    attackSpinBox->setValue(m_cachedSynthConfig[index].attackMsec);
                    decaySpinBox->setValue(m_cachedSynthConfig[index].decayMsec);
                    decayRatioSpinBox->setValue(m_cachedSynthConfig[index].decayRatio);
                    releaseSpinBox->setValue(m_cachedSynthConfig[index].releaseMsec);

                    amplitudeSlider->setDefaultValue(
                        amplitudeDefaultValue[m_cachedSynthConfig[index].generator]);

                    m_oldSynthIndex = index;
                });

        readPitchCheckBox->setChecked(m_cachedReadPitch = AudioSettings::pseudoSingerReadPitch());
        readEnergyCheckBox->setChecked(m_cachedReadEnergy =
                                           AudioSettings::pseudoSingerReadEnergy());

        connect(readPitchCheckBox, &QAbstractButton::clicked, this,
                [this](const bool checked) { m_cachedReadPitch = checked; });
        connect(readEnergyCheckBox, &QAbstractButton::clicked, this,
                [this](const bool checked) { m_cachedReadEnergy = checked; });
    }

    void accept() {
        m_cachedSynthConfig[m_oldSynthIndex].generator = d->m_cachedGenerator;
        m_cachedSynthConfig[m_oldSynthIndex].amplitude = d->m_cachedAmplitude;
        m_cachedSynthConfig[m_oldSynthIndex].attackMsec = d->m_cachedAttackMsec;
        m_cachedSynthConfig[m_oldSynthIndex].decayMsec = d->m_cachedDecayMsec;
        m_cachedSynthConfig[m_oldSynthIndex].decayRatio = d->m_cachedDecayRatio;
        m_cachedSynthConfig[m_oldSynthIndex].releaseMsec = d->m_cachedReleaseMsec;

        for (int i = 0; i < 4; i++) {
            AudioSettings::setPseudoSingerSynthGenerator(i, m_cachedSynthConfig[i].generator);
            AudioSettings::setPseudoSingerSynthAmplitude(i, m_cachedSynthConfig[i].amplitude);
            AudioSettings::setPseudoSingerSynthAttackMsec(i, m_cachedSynthConfig[i].attackMsec);
            AudioSettings::setPseudoSingerSynthDecayMsec(i, m_cachedSynthConfig[i].decayMsec);
            AudioSettings::setPseudoSingerSynthDecayRatio(i, m_cachedSynthConfig[i].decayRatio);
            AudioSettings::setPseudoSingerSynthReleaseMsec(i, m_cachedSynthConfig[i].releaseMsec);

            PseudoSingerConfigNotifier::notify(i);
        }
        AudioSettings::setPseudoSingerReadPitch(m_cachedReadPitch);
        AudioSettings::setPseudoSingerReadEnergy(m_cachedReadEnergy);
    }

    struct {
        int generator;
        double amplitude;
        int attackMsec;
        int decayMsec;
        double decayRatio;
        int releaseMsec;
    } m_cachedSynthConfig[4];

    bool m_cachedReadPitch;
    bool m_cachedReadEnergy;

    SettingPageSynthHelper *d;
    int m_oldSynthIndex = 0;
};

PseudoSingerPage::PseudoSingerPage(QWidget *parent) : IOptionPage(parent) {
    initializePage();
}

PseudoSingerPage::~PseudoSingerPage() {
    PseudoSingerPage::modifyOption();
}

void PseudoSingerPage::modifyOption() {
    m_widget->accept();
}

QWidget * PseudoSingerPage::createContentWidget() {
    m_widget = new PseudoSingerPageWidget;
    m_widget->setContentsMargins({});
    return m_widget;
}

#include "PseudoSingerPage.moc"
