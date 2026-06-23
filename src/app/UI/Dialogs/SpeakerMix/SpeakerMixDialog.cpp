#include "SpeakerMixDialog.h"
#include "SpeakerMixBar.h"
#include "SpeakerMixList.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/FlowLayout.h"
#include "UI/Controls/TagButton.h"
#include "UI/Controls/Toast.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

using namespace SpeakerMixModel;

SpeakerMixDialog::SpeakerMixDialog(const SingerInfo &singerInfo, const SpeakerMixData &mixData,
                                   QWidget *parent)
    : OKCancelDialog(parent), m_singerInfo(singerInfo),
      m_initialData(normalizeSpeakerMixData(mixData)) {
    setWindowTitle("Speaker Mix");
    setTitle("Speaker Mix");
    setMinimumWidth(480);
    setMinimumHeight(312);

    QStringList speakerTypes;
    QMap<QString, QString> speakerDisplayNames;
    for (const auto &speaker : m_singerInfo.speakers()) {
        speakerTypes.append(speaker.id());
        speakerDisplayNames.insert(speaker.id(),
                                   speaker.name().isEmpty() ? speaker.id() : speaker.name());
    }

    m_mixList =
        new SpeakerMixList(m_singerInfo.name(), speakerTypes, m_singerInfo.speakers(), this);
    m_mixList->setSpeakerDisplayNames(speakerDisplayNames);

    const auto tagContainer = new QWidget(this);
    tagContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    const auto tagLayout = new FlowLayout(tagContainer, 0, 6, 6);

    for (int i = 0; i < speakerTypes.size(); ++i) {
        const QString &name = speakerTypes[i];

        const auto btn = new TagButton(speakerDisplayNames.value(name, name), tagContainer);
        btn->setChecked(true);

        btn->setProperty("speakerName", name);
        connect(btn, &TagButton::toggled, this, &SpeakerMixDialog::onTagToggled);
        tagLayout->addWidget(btn);
        m_tagButtons[name] = btn;
    }

    connect(m_mixList, &SpeakerMixList::speakerChanged, this,
            &SpeakerMixDialog::onSpeakerChangedInList);
    connect(okButton(), &AccentButton::clicked, this, [this] {
        m_result = buildCurrentSpeakerMixData();
        accept();
    });
    connect(cancelButton(), &Button::clicked, this, &Dialog::reject);

    setupInitialSources(mixData);

    const auto layout = new QVBoxLayout(body());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(buildPresetBar());
    layout->addWidget(tagContainer);
    layout->addWidget(m_mixList);
    layout->addWidget(m_mixList->getMixBar());
    reloadPresetCombo();
}

SpeakerMixData SpeakerMixDialog::speakerMixData() const {
    return m_result;
}

void SpeakerMixDialog::setupInitialSources(const SpeakerMixData &mixData) {
    QVector<double> fullWeights;

    if (mixData.mode != SingerSourceMode::Single && mixData.sources.size() >= 2) {
        QVector<QString> sourceIds;
        QVector<int> sourceIndexes;
        sourceIds.reserve(mixData.sources.size());
        sourceIndexes.reserve(mixData.sources.size());

        for (int i = 0; i < mixData.sources.size(); ++i) {
            const auto id = mixData.sources[i].speaker.id();
            if (!m_tagButtons.contains(id) || sourceIds.contains(id))
                continue;
            sourceIds.append(id);
            sourceIndexes.append(i);
        }

        if (sourceIds.size() >= 2) {
            for (const auto &id : std::as_const(sourceIds))
                m_mixList->addSpeaker(id);

            QVector<double> explicitWeights = mixData.fixedWeights;
            if (explicitWeights.size() != mixData.sources.size() - 1 &&
                !mixData.dynamicKeyframes.isEmpty()) {
                explicitWeights = mixData.dynamicKeyframes.first().weights;
            }
            if (explicitWeights.size() == mixData.sources.size() - 1) {
                const auto sourceWeights = fullWeightsFromExplicitWeights(explicitWeights);
                if (sourceWeights.size() == mixData.sources.size()) {
                    fullWeights.reserve(sourceIndexes.size());
                    for (const int sourceIndex : std::as_const(sourceIndexes))
                        fullWeights.append(sourceWeights[sourceIndex]);
                    fullWeights = normalizeSpeakerMixFullWeights(fullWeights, fullWeights.size());
                }
            }
        }
    }

    if (m_mixList->getLabels().isEmpty()) {
        for (const auto &speaker : m_singerInfo.speakers()) {
            m_mixList->addSpeaker(speaker.id());
        }
    } else if (fullWeights.size() == m_mixList->getLabels().size()) {
        QVector<double> percentages;
        percentages.reserve(fullWeights.size());
        for (const double weight : fullWeights)
            percentages.append(weight * 100.0);
        m_mixList->setDoubleValues(percentages);
    }

    updateTagStates();
}

QWidget *SpeakerMixDialog::buildPresetBar() {
    const auto container = new QWidget(this);
    const auto layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_cbPresets = new ComboBox(container);
    m_cbPresets->setMinimumWidth(160);
    connect(m_cbPresets, &QComboBox::currentIndexChanged, this,
            &SpeakerMixDialog::onPresetSelected);

    m_btnNew = new Button(tr("New"), container);
    m_btnSave = new Button(tr("Save"), container);
    m_btnSaveAs = new Button(tr("Save As"), container);
    m_btnDelete = new Button(tr("Delete"), container);
    m_btnReset = new Button(tr("Reset"), container);

    connect(m_btnNew, &Button::clicked, this, &SpeakerMixDialog::onNewPreset);
    connect(m_btnSave, &Button::clicked, this, &SpeakerMixDialog::onSavePreset);
    connect(m_btnSaveAs, &Button::clicked, this, &SpeakerMixDialog::onSaveAsPreset);
    connect(m_btnDelete, &Button::clicked, this, &SpeakerMixDialog::onDeletePreset);
    connect(m_btnReset, &Button::clicked, this, &SpeakerMixDialog::onResetPreset);

    layout->addWidget(m_cbPresets, 1);
    layout->addWidget(m_btnNew);
    layout->addWidget(m_btnSave);
    layout->addWidget(m_btnSaveAs);
    layout->addWidget(m_btnDelete);
    layout->addWidget(m_btnReset);
    return container;
}

void SpeakerMixDialog::applySpeakerMixDataToUi(const SpeakerMixData &mixData) {
    QVector<QString> sourceIds;
    QVector<int> sourceIndexes;
    sourceIds.reserve(mixData.sources.size());
    sourceIndexes.reserve(mixData.sources.size());

    if (mixData.mode != SingerSourceMode::Single) {
        for (int i = 0; i < mixData.sources.size(); ++i) {
            const auto id = mixData.sources[i].speaker.id();
            if (!m_tagButtons.contains(id) || sourceIds.contains(id))
                continue;
            sourceIds.append(id);
            sourceIndexes.append(i);
        }
    }

    if (sourceIds.size() < 2) {
        for (const auto &speaker : m_singerInfo.speakers())
            sourceIds.append(speaker.id());
        m_mixList->setSpeakers(sourceIds);
        updateTagStates();
        return;
    }

    m_mixList->setSpeakers(sourceIds);
    QVector<double> weights = mixData.fixedWeights;
    if (weights.size() != mixData.sources.size() - 1 && !mixData.dynamicKeyframes.isEmpty())
        weights = mixData.dynamicKeyframes.first().weights;
    if (weights.size() == mixData.sources.size() - 1) {
        const auto sourceWeights = fullWeightsFromExplicitWeights(weights);
        QVector<double> percentages;
        percentages.reserve(sourceIndexes.size());
        for (const int sourceIndex : std::as_const(sourceIndexes))
            percentages.append(sourceWeights.value(sourceIndex) * 100.0);
        m_mixList->setDoubleValues(percentages);
    }
    updateTagStates();
}

void SpeakerMixDialog::reloadPresetCombo(const QString &selectedId) {
    if (!m_cbPresets)
        return;

    const QSignalBlocker blocker(m_cbPresets);
    m_cbPresets->clear();
    m_cbPresets->addItem(tr("No preset"), QString());
    const auto presets = SpeakerMixPresetStore::presetsForSinger(m_singerInfo);
    int selectedIndex = 0;
    for (const auto &preset : presets) {
        m_cbPresets->addItem(preset.name, preset.id);
        if (preset.id == selectedId)
            selectedIndex = m_cbPresets->count() - 1;
    }
    m_cbPresets->setCurrentIndex(selectedIndex);
    m_currentPresetId = m_cbPresets->currentData().toString();
    m_isNewPreset = m_currentPresetId.isEmpty();
    updatePresetButtons();
}

bool SpeakerMixDialog::applyPresetToUi(const QString &presetId, const bool showWarning) {
    const auto preset = SpeakerMixPresetStore::findPreset(presetId);
    if (!preset || preset->singerIdentifier() != m_singerInfo.identifier())
        return false;

    SpeakerMixData data;
    data.mode = SingerSourceMode::FixedMix;
    for (const auto &source : preset->sources) {
        const auto speaker = speakerById(source.speaker.id());
        if (!speaker.isEmpty())
            data.sources.append({speaker});
    }
    data.fixedWeights = preset->fixedWeights;
    data = normalizeSpeakerMixData(data);
    if (isSpeakerMixDataSingle(data)) {
        if (showWarning)
            Toast::show(tr("Preset speakers are unavailable"));
        return false;
    }

    applySpeakerMixDataToUi(data);
    return true;
}

SpeakerMixData SpeakerMixDialog::buildCurrentSpeakerMixData() const {
    SpeakerMixData data;
    data.mode = SingerSourceMode::FixedMix;
    const auto labels = m_mixList->getLabels();
    for (const auto &label : labels) {
        auto speaker = speakerById(label);
        if (!speaker.isEmpty())
            data.sources.append({speaker});
    }
    data.fixedWeights = explicitWeightsFromFullWeights(currentFullWeights());
    data = normalizeSpeakerMixData(data);
    if (!m_currentPresetId.isEmpty()) {
        if (const auto preset = SpeakerMixPresetStore::findPreset(m_currentPresetId)) {
            data.sourcePresetId = preset->id;
            data.sourcePresetName = preset->name;
            data.sourcePresetDirty =
                !SpeakerMixPresetStore::speakerMixDataMatchesPreset(*preset, m_singerInfo, data);
            data = normalizeSpeakerMixData(data);
        }
    }
    return data;
}

SpeakerMixData SpeakerMixDialog::equalWeightMixData() const {
    SpeakerMixData data;
    data.mode = SingerSourceMode::FixedMix;
    QVector<double> weights;
    for (const auto &speaker : m_singerInfo.speakers()) {
        data.sources.append({speaker});
        weights.append(1.0);
    }
    data.fixedWeights = explicitWeightsFromFullWeights(weights);
    return normalizeSpeakerMixData(data);
}

void SpeakerMixDialog::updatePresetButtons() const {
    const bool hasPreset = !m_currentPresetId.isEmpty() && !m_isNewPreset;
    if (m_btnSave)
        m_btnSave->setEnabled(hasPreset);
    if (m_btnDelete)
        m_btnDelete->setEnabled(hasPreset);
}

void SpeakerMixDialog::onPresetSelected(const int index) {
    if (!m_cbPresets)
        return;

    const QString presetId = m_cbPresets->itemData(index).toString();
    if (presetId.isEmpty()) {
        m_currentPresetId.clear();
        m_isNewPreset = true;
        updatePresetButtons();
        return;
    }

    if (applyPresetToUi(presetId, true)) {
        m_currentPresetId = presetId;
        m_isNewPreset = false;
    }
    updatePresetButtons();
}

void SpeakerMixDialog::onNewPreset() {
    m_currentPresetId.clear();
    m_isNewPreset = true;
    if (m_cbPresets)
        m_cbPresets->setCurrentIndex(0);
    applySpeakerMixDataToUi(equalWeightMixData());
    updatePresetButtons();
}

void SpeakerMixDialog::onSavePreset() {
    if (m_currentPresetId.isEmpty())
        return;

    auto preset = SpeakerMixPresetStore::findPreset(m_currentPresetId);
    if (!preset)
        return;

    const auto data = buildCurrentSpeakerMixData();
    if (isSpeakerMixDataSingle(data))
        return;

    preset->sources = data.sources;
    preset->fixedWeights = data.fixedWeights;
    if (SpeakerMixPresetStore::savePreset(*preset))
        reloadPresetCombo(preset->id);
}

void SpeakerMixDialog::onSaveAsPreset() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, {}, tr("Preset name"), QLineEdit::Normal, {}, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    if (SpeakerMixPresetStore::presetNameExists(m_singerInfo, name)) {
        Toast::show(tr("Preset name already exists"));
        return;
    }

    const auto data = buildCurrentSpeakerMixData();
    if (isSpeakerMixDataSingle(data))
        return;

    SpeakerMixPreset preset;
    preset.name = name;
    preset.packageId = m_singerInfo.packageId();
    preset.singerId = m_singerInfo.singerId();
    preset.packageVersion = m_singerInfo.packageVersion();
    preset.sources = data.sources;
    preset.fixedWeights = data.fixedWeights;
    if (SpeakerMixPresetStore::savePreset(preset))
        reloadPresetCombo(SpeakerMixPresetStore::findPresetByName(m_singerInfo, name)->id);
}

void SpeakerMixDialog::onDeletePreset() {
    if (m_currentPresetId.isEmpty())
        return;
    if (QMessageBox::question(this, {}, tr("Delete this preset?")) != QMessageBox::Yes)
        return;
    SpeakerMixPresetStore::deletePreset(m_currentPresetId);
    reloadPresetCombo();
    applySpeakerMixDataToUi(m_initialData);
}

void SpeakerMixDialog::onResetPreset() {
    if (!m_currentPresetId.isEmpty() && applyPresetToUi(m_currentPresetId, true))
        return;
    applySpeakerMixDataToUi(m_initialData);
}

void SpeakerMixDialog::onTagToggled(bool checked) {
    const auto btn = qobject_cast<TagButton *>(sender());
    if (!btn)
        return;

    const QString name = btn->property("speakerName").toString();

    if (checked) {
        m_mixList->addSpeaker(name);
    } else {
        if (m_mixList->getLabels().size() <= 1) {
            const QSignalBlocker blocker(btn);
            btn->setChecked(true);
            return;
        }
        m_mixList->removeSpeaker(name);
    }
}

void SpeakerMixDialog::onSpeakerChangedInList(const QString &oldName, const QString &newName) {
    updateTagStates();
}

void SpeakerMixDialog::updateTagStates() {
    const QVector<QString> activeLabels = m_mixList->getLabels();

    for (auto it = m_tagButtons.begin(); it != m_tagButtons.end(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setChecked(activeLabels.contains(it.key()));
    }
}

QVector<double> SpeakerMixDialog::currentFullWeights() const {
    QVector<double> result;
    const auto values = m_mixList->getMixBar()->getDoubleValues();
    result.reserve(values.size());
    for (const double value : values)
        result.append(value / 100.0);
    return result;
}

SpeakerInfo SpeakerMixDialog::speakerById(const QString &id) const {
    for (const auto &speaker : m_singerInfo.speakers()) {
        if (speaker.id() == id)
            return speaker;
    }
    return {};
}
