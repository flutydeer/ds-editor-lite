#include "MidiPage.h"

#include <cmath>

#include <QDebug>
#include <QBoxLayout>
#include <QMessageBox>
#include <QScrollBar>
#include <QEvent>
#include <QSignalBlocker>
#include <QPushButton>

#include "UI/Controls/HorizontalPianoKeyboardView.h"

#include <TalcsCore/NoteSynthesizer.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsCore/Decibels.h>
#include <TalcsMidi/MidiInputDevice.h>
#include <TalcsMidi/MidiNoteSynthesizer.h>

#include "Utils/Decibellinearizer.h"
#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>
#include <lite/GUI/Controls/OptionListCard.h>
#include <lite/GUI/Controls/OptionsCardItem.h>
#include <lite/GUI/Controls/SwitchButton.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/WheelEventPolicy.h>

#include <Modules/Audio/AudioSystem.h>

#include <Modules/Audio/subsystem/MidiSystem.h>
#include <Modules/Audio/utils/AudioHelpers.h>
#include <Modules/Audio/utils/MidiInputNoteMonitor.h>
#include <Modules/Audio/utils/SettingPagesSynthHelper.h>

QWidget *MidiPage::createContentWidget() {
    const auto widget = new QWidget;
    m_synthHelper = new SettingPageSynthHelper(this);
    const auto mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins({});

    m_deviceComboBox = new ComboBox;
    m_inputCard = new OptionListCard(tr("MIDI Input"));
    m_deviceItem = m_inputCard->addItem(tr("&Device"), m_deviceComboBox);
    mainLayout->addWidget(m_inputCard);

    m_generatorComboBox = new ComboBox;
    m_generatorComboBox->setObjectName("midiGenerator");
    m_generatorComboBox->addItem(tr("Sine wave"), talcs::NoteSynthesizer::Sine);
    m_generatorComboBox->addItem(tr("Square wave"), talcs::NoteSynthesizer::Square);
    m_generatorComboBox->addItem(tr("Triangle Wave"), talcs::NoteSynthesizer::Triangle);
    m_generatorComboBox->addItem(tr("Sawtooth wave"), talcs::NoteSynthesizer::Sawtooth);

    m_amplitudeSlider = new SVS::SeekBar;
    m_amplitudeSlider->setFixedWidth(256);
    m_amplitudeSlider->setDefaultValue(DecibelLinearizer::decibelToLinearValue(-3));
    m_amplitudeSlider->setRange(DecibelLinearizer::decibelToLinearValue(-96),
                                DecibelLinearizer::decibelToLinearValue(0));
    m_amplitudeSpinBox = new SVS::ExpressionDoubleSpinBox;
    m_amplitudeSpinBox->setObjectName("midiAmplitude");
    m_amplitudeSpinBox->setDecimals(1);
    m_amplitudeSpinBox->setRange(-96, 0);
    m_amplitudeSpinBox->setSpecialValueText("-INF");
    // Prevent accidental value changes while scrolling the settings page.
    m_amplitudeSpinBox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_amplitudeSpinBox->setFocusPolicy(Qt::StrongFocus);

    m_attackSlider = new SVS::SeekBar;
    m_attackSlider->setFixedWidth(256);
    m_attackSlider->setInterval(1);
    m_attackSlider->setDefaultValue(10);
    m_attackSlider->setRange(0, 100);
    m_attackSpinBox = new SVS::ExpressionSpinBox;
    m_attackSpinBox->setObjectName("midiAttack");
    m_attackSpinBox->setRange(0, 100);
    // Prevent accidental value changes while scrolling the settings page.
    m_attackSpinBox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_attackSpinBox->setFocusPolicy(Qt::StrongFocus);

    m_decaySlider = new SVS::SeekBar;
    m_decaySlider->setFixedWidth(256);
    m_decaySlider->setInterval(1);
    m_decaySlider->setDefaultValue(1000);
    m_decaySlider->setRange(0, 1000);
    m_decaySpinBox = new SVS::ExpressionSpinBox;
    m_decaySpinBox->setObjectName("midiDecay");
    m_decaySpinBox->setRange(0, 1000);
    // Prevent accidental value changes while scrolling the settings page.
    m_decaySpinBox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_decaySpinBox->setFocusPolicy(Qt::StrongFocus);

    m_decayRatioSlider = new SVS::SeekBar;
    m_decayRatioSlider->setFixedWidth(256);
    m_decayRatioSlider->setDefaultValue(0.5);
    m_decayRatioSlider->setRange(0, 1);
    m_decayRatioSpinBox = new SVS::ExpressionDoubleSpinBox;
    m_decayRatioSpinBox->setObjectName("midiDecayRatio");
    m_decayRatioSpinBox->setRange(0, 1);
    // Prevent accidental value changes while scrolling the settings page.
    m_decayRatioSpinBox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_decayRatioSpinBox->setFocusPolicy(Qt::StrongFocus);

    m_releaseSlider = new SVS::SeekBar;
    m_releaseSlider->setFixedWidth(256);
    m_releaseSlider->setInterval(1);
    m_releaseSlider->setDefaultValue(50);
    m_releaseSlider->setRange(0, 100);
    m_releaseSpinBox = new SVS::ExpressionSpinBox;
    m_releaseSpinBox->setObjectName("midiRelease");
    m_releaseSpinBox->setRange(0, 100);
    // Prevent accidental value changes while scrolling the settings page.
    m_releaseSpinBox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_releaseSpinBox->setFocusPolicy(Qt::StrongFocus);

    m_frequencyOfASpinBox = new SVS::ExpressionDoubleSpinBox;
    m_frequencyOfASpinBox->setObjectName("midiFrequencyOfA");
    m_frequencyOfASpinBox->setRange(440.0 * std::pow(2, -1.0 / 24.0),
                                    440.0 * std::pow(2, 1.0 / 24.0));
    // Prevent accidental value changes while scrolling the settings page.
    m_frequencyOfASpinBox->setWheelEventPolicy(WheelEventPolicy::Consume);
    m_frequencyOfASpinBox->setFocusPolicy(Qt::StrongFocus);
    m_adjustByProjectSwitch = new SwitchButton;
    m_adjustByProjectSwitch->setObjectName("midiAdjustByProject");

    m_synthesizerCard = new OptionListCard(tr("Synthesizer"));
    m_generatorItem = m_synthesizerCard->addItem(tr("&Generator"), m_generatorComboBox);
    m_amplitudeItem =
        m_synthesizerCard->addItem(tr("&Amplitude (dB)"), {m_amplitudeSlider, m_amplitudeSpinBox});
    m_attackItem =
        m_synthesizerCard->addItem(tr("A&ttack (ms)"), {m_attackSlider, m_attackSpinBox});
    m_decayItem = m_synthesizerCard->addItem(tr("D&ecay (ms)"), {m_decaySlider, m_decaySpinBox});
    m_decayRatioItem =
        m_synthesizerCard->addItem(tr("Decay rati&o"), {m_decayRatioSlider, m_decayRatioSpinBox});
    m_releaseItem =
        m_synthesizerCard->addItem(tr("&Release (ms)"), {m_releaseSlider, m_releaseSpinBox});
    m_frequencyItem = m_synthesizerCard->addItem(tr("&Frequency of A"), m_frequencyOfASpinBox);
    m_adjustByProjectItem =
        m_synthesizerCard->addItem(tr("Adjust by project cent shift"), m_adjustByProjectSwitch);
    m_flushButton = new QPushButton(tr("&Interrupt All Notes"));
    m_flushButton->setObjectName("midiInterruptAllNotes");
    m_flushButton->setToolTip(
        tr("Interrupt all notes that are currently played by the synthesizer"));
    m_controlItem = m_synthesizerCard->addItem(tr("Control"), m_flushButton);
    connect(m_flushButton, &QPushButton::clicked, this, [this] {
        AudioSystem::midiSystem()->synthesizer()->noteSynthesizer()->flush();
        m_previewKeyboard->clearHeldKeys();
    });
    mainLayout->addWidget(m_synthesizerCard);

    // Preview keyboard. The key bed has a fixed natural size, so the card only
    // scrolls when the window is too narrow for it. The bar is the plain widget
    // one: an overlay bar would be painted over the keys.
    m_previewKeyboard = new HorizontalPianoKeyboardView;
    m_previewKeyboard->setObjectName("midiPreviewKeyboard");
    m_keyboardScroll = new QScrollArea;
    m_keyboardScroll->setObjectName("midiPreviewKeyboardScroll");
    m_keyboardScroll->setFrameShape(QFrame::NoFrame);
    m_keyboardScroll->setWidgetResizable(false);
    m_keyboardScroll->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_keyboardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_keyboardScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_keyboardScroll->setFocusPolicy(Qt::NoFocus);
    m_keyboardScroll->setWidget(m_previewKeyboard);

    // Reserve the bar's band only while it can actually scroll, so the card
    // stays tight at the default window size.
    const auto syncKeyboardScrollHeight = [this] {
        const auto *bar = m_keyboardScroll->horizontalScrollBar();
        const auto barHeight = bar->maximum() > bar->minimum() ? bar->sizeHint().height() : 0;
        m_keyboardScroll->setFixedHeight(m_previewKeyboard->height() + barHeight);
    };
    syncKeyboardScrollHeight();
    connect(m_keyboardScroll->horizontalScrollBar(), &QScrollBar::rangeChanged, this,
            [syncKeyboardScrollHeight](int, int) { syncKeyboardScrollHeight(); });

    m_keyboardCard = new OptionsCard;
    m_keyboardCard->setTitle(tr("Keyboard"));
    const auto keyboardLayout = new QVBoxLayout;
    keyboardLayout->setContentsMargins(10, 5, 10, 5);
    keyboardLayout->setSpacing(0);
    keyboardLayout->addWidget(m_keyboardScroll);
    m_keyboardCard->card()->setLayout(keyboardLayout);
    mainLayout->addWidget(m_keyboardCard);

    connect(m_previewKeyboard, &HorizontalPianoKeyboardView::noteOn, this,
            [this](const int key) { m_synthHelper->noteOn(key); });
    connect(m_previewKeyboard, &HorizontalPianoKeyboardView::noteOff, this,
            [this](const int key) { m_synthHelper->noteOff(key); });

    // Mirror an attached MIDI keyboard. The device reports on its own thread, so
    // these connections are queued and land on the GUI thread.
    connect(m_noteMonitor, &MidiInputNoteMonitor::notePressed, m_previewKeyboard,
            [this](const int key) { m_previewKeyboard->setHeldKey(key, true); });
    connect(m_noteMonitor, &MidiInputNoteMonitor::noteReleased, m_previewKeyboard,
            [this](const int key) { m_previewKeyboard->setHeldKey(key, false); });
    connect(m_noteMonitor, &MidiInputNoteMonitor::allNotesReleased, m_previewKeyboard,
            [this] { m_previewKeyboard->clearHeldKeys(); });

    mainLayout->addStretch();
    widget->setLayout(mainLayout);
    widget->setContentsMargins({});

    auto ms = AudioSystem::midiSystem();

    const auto deviceList = MidiSystem::availableDevices();
    if (!ms->device()) {
        m_deviceComboBox->addItem(tr("(Not working)"), -1);
    }
    for (int i = 0; i < deviceList.size(); i++) {
        m_deviceComboBox->addItem(deviceList.at(i), i);
        if (ms->device() && i == ms->device()->deviceIndex())
            m_deviceComboBox->setCurrentIndex(i);
    }
    connect(m_deviceComboBox, QOverload<int>::of(&ComboBox::currentIndexChanged), this,
            [ms, this](const int index) {
                const auto i = m_deviceComboBox->itemData(index).toInt();
                if (i == -1)
                    return;
                if (!ms->setDevice(i)) {
                    QMessageBox::critical(
                        this, {},
                        tr("Cannot open MIDI device %1").arg(m_deviceComboBox->itemText(index)));
                    QSignalBlocker o(m_deviceComboBox);
                    m_deviceComboBox->setCurrentIndex(m_deviceComboBox->findData(
                        ms->device() ? ms->device()->deviceIndex() : -1));
                } else {
                    QSignalBlocker o(m_deviceComboBox);
                    if (const auto notWorkingIndex = m_deviceComboBox->findData(-1);
                        notWorkingIndex != -1)
                        m_deviceComboBox->removeItem(notWorkingIndex);
                }
            });

    m_synthHelper->m_cachedGenerator = ms->generator();
    m_synthHelper->m_cachedAmplitude = ms->amplitudeDecibel();
    m_synthHelper->m_cachedAttackMsec = ms->attackMsec();
    m_synthHelper->m_cachedDecayMsec = ms->decayMsec();
    m_synthHelper->m_cachedDecayRatio = ms->decayRatio();
    m_synthHelper->m_cachedReleaseMsec = ms->releaseMsec();
    m_synthHelper->m_cachedFrequencyOfA = ms->frequencyOfA();

    m_synthHelper->initialize(m_generatorComboBox, m_amplitudeSlider, m_amplitudeSpinBox,
                              m_attackSlider, m_attackSpinBox, m_decaySlider, m_decaySpinBox,
                              m_decayRatioSlider, m_decayRatioSpinBox, m_releaseSlider,
                              m_releaseSpinBox);

    connect(m_frequencyOfASpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](const double value) {
                m_synthHelper->m_mutex.lock();
                if (!m_adjustByProjectSwitch->value())
                    m_synthHelper->m_cachedFrequencyOfA = value;
                m_synthHelper->m_mutex.unlock();
                m_synthHelper->m_testSynthesizer.flush();
                m_synthHelper->flushPreviewNotes();
            });
    connect(m_adjustByProjectSwitch, &SwitchButton::toggled, this, [this](const bool checked) {
        m_synthHelper->m_mutex.lock();
        if (checked) {
            m_synthHelper->m_cachedFrequencyOfA = 0;
            m_frequencyOfASpinBox->setDisabled(true);
        } else {
            m_synthHelper->m_cachedFrequencyOfA = m_frequencyOfASpinBox->value();
            m_frequencyOfASpinBox->setDisabled(false);
        }
        m_synthHelper->m_mutex.unlock();
        m_synthHelper->m_testSynthesizer.flush();
        m_synthHelper->flushPreviewNotes();
    });

    m_synthHelper->m_cachedFrequencyOfA = ms->frequencyOfA();
    if (qFuzzyIsNull(m_synthHelper->m_cachedFrequencyOfA)) {
        m_adjustByProjectSwitch->setValue(true);
        m_frequencyOfASpinBox->setDisabled(true);
        m_frequencyOfASpinBox->setValue(440.0);
    } else {
        m_frequencyOfASpinBox->setValue(m_synthHelper->m_cachedFrequencyOfA);
    }

    return widget;
}

MidiPage::MidiPage(QWidget *parent) : IOptionPage(parent) {
    // One monitor per page: the content widget (and the keyboard with it) is
    // rebuilt on a language change, while the monitor has to keep watching the
    // MIDI input for the new one.
    m_noteMonitor = new MidiInputNoteMonitor(this);
    m_noteMonitor->attachTo(AudioSystem::midiSystem()->integrator());
    initializePage();
}

MidiPage::~MidiPage() {
    MidiPage::modifyOption();
}

void MidiPage::modifyOption() {
    const auto ms = AudioSystem::midiSystem();
    ms->setGenerator(m_synthHelper->m_cachedGenerator);
    ms->setAmplitudeDecibel(m_synthHelper->m_cachedAmplitude);
    ms->setAttackMsec(m_synthHelper->m_cachedAttackMsec);
    ms->setDecayMsec(m_synthHelper->m_cachedDecayMsec);
    ms->setDecayRatio(m_synthHelper->m_cachedDecayRatio);
    ms->setReleaseMsec(m_synthHelper->m_cachedReleaseMsec);
    ms->setFrequencyOfA(m_synthHelper->m_cachedFrequencyOfA);
}

void MidiPage::changeEvent(QEvent *event) {
    QScrollArea::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void MidiPage::retranslateUi() {
    m_inputCard->setTitle(tr("MIDI Input"));
    m_deviceItem->setTitle(tr("&Device"));
    if (const auto notWorkingIndex = m_deviceComboBox->findData(-1); notWorkingIndex >= 0)
        m_deviceComboBox->setItemText(notWorkingIndex, tr("(Not working)"));

    m_synthesizerCard->setTitle(tr("Synthesizer"));
    m_generatorItem->setTitle(tr("&Generator"));
    m_amplitudeItem->setTitle(tr("&Amplitude (dB)"));
    m_attackItem->setTitle(tr("A&ttack (ms)"));
    m_decayItem->setTitle(tr("D&ecay (ms)"));
    m_decayRatioItem->setTitle(tr("Decay rati&o"));
    m_releaseItem->setTitle(tr("&Release (ms)"));
    m_frequencyItem->setTitle(tr("&Frequency of A"));
    m_adjustByProjectItem->setTitle(tr("Adjust by project cent shift"));

    m_keyboardCard->setTitle(tr("Keyboard"));
    m_controlItem->setTitle(tr("Control"));
    m_flushButton->setText(tr("&Interrupt All Notes"));
    m_flushButton->setToolTip(
        tr("Interrupt all notes that are currently played by the synthesizer"));

    const auto generator = m_generatorComboBox->currentData();
    const QSignalBlocker blocker(m_generatorComboBox);
    m_generatorComboBox->clear();
    m_generatorComboBox->addItem(tr("Sine wave"), talcs::NoteSynthesizer::Sine);
    m_generatorComboBox->addItem(tr("Square wave"), talcs::NoteSynthesizer::Square);
    m_generatorComboBox->addItem(tr("Triangle Wave"), talcs::NoteSynthesizer::Triangle);
    m_generatorComboBox->addItem(tr("Sawtooth wave"), talcs::NoteSynthesizer::Sawtooth);
    m_generatorComboBox->setCurrentIndex(m_generatorComboBox->findData(generator));
}
