#include "SpeakerMixDialog.h"
#include "SpeakerMixBar.h"
#include "SpeakerMixList.h"
#include "Model/SpeakerMixPreset/SpeakerMixPresetStore.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ComboBox.h"
#include "UI/Controls/FlowLayout.h"
#include "UI/Controls/Menu.h"
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
    : OKCancelDialog(parent), m_singerInfo(singerInfo) {
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

    // 全选按钮
    const auto btnSelectAll = new Button(tr("All"), tagContainer);
    btnSelectAll->setCursor(Qt::PointingHandCursor);
    connect(btnSelectAll, &Button::clicked, this, [this] {
        for (auto it = m_tagButtons.begin(); it != m_tagButtons.end(); ++it) {
            if (!it.value()->isChecked())
                it.value()->setChecked(true);
        }
    });
    tagLayout->addWidget(btnSelectAll);

    // 反选按钮
    const auto btnInvert = new Button(tr("Invert"), tagContainer);
    btnInvert->setCursor(Qt::PointingHandCursor);
    connect(btnInvert, &Button::clicked, this, [this] {
        QVector<QString> targetIds;
        for (auto it = m_tagButtons.begin(); it != m_tagButtons.end(); ++it) {
            if (!it.value()->isChecked())
                targetIds.append(it.key());
        }
        if (targetIds.isEmpty())
            targetIds.append(m_tagButtons.firstKey());
        m_mixList->setSpeakers(targetIds);
        updateTagStates();
    });
    tagLayout->addWidget(btnInvert);

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
    reloadPresetCombo(mixData.sourcePresetId);

    // If combo shows Init Preset (no saved preset), align sliders to equal weight
    if (m_currentPresetId.isEmpty())
        applySpeakerMixDataToUi(equalWeightMixData());
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

    m_btnSaveAs = new Button(tr("Save As"), container);
    connect(m_btnSaveAs, &Button::clicked, this, &SpeakerMixDialog::onSaveAsPreset);

    m_btnMenu = new Button(tr("More..."), container);
    const auto menu = new Menu(m_btnMenu);
    menu->addAction(tr("Initialize"), this, &SpeakerMixDialog::onInitializePreset);
    m_deleteAction = menu->addAction(tr("Delete"), this, &SpeakerMixDialog::onDeletePreset);
    connect(m_btnMenu, &Button::clicked, this, [this, menu] {
        m_deleteAction->setEnabled(!m_currentPresetId.isEmpty());
        menu->exec(m_btnMenu->mapToGlobal(QPoint(0, m_btnMenu->height())));
    });

    layout->addWidget(m_cbPresets, 1);
    layout->addWidget(m_btnSaveAs);
    layout->addWidget(m_btnMenu);
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
    m_cbPresets->addItem(tr("Init Preset"), QString());
    const auto presets = SpeakerMixPresetStore::presetsForSinger(m_singerInfo);
    int selectedIndex = 0;
    for (const auto &preset : presets) {
        m_cbPresets->addItem(preset.name, preset.id);
        if (preset.id == selectedId)
            selectedIndex = m_cbPresets->count() - 1;
    }
    m_cbPresets->setCurrentIndex(selectedIndex);
    m_currentPresetId = m_cbPresets->currentData().toString();
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
    const auto speakers = m_singerInfo.speakers();
    const int n = speakers.size();
    if (n < 2)
        return data;

    // Match SpeakerMixList::redistributeValues() — integer 100/n + remainder
    const int baseValue = 100 / n;
    const int remainder = 100 % n;
    QVector<double> fullWeights;
    fullWeights.reserve(n);
    for (int i = 0; i < n; ++i) {
        data.sources.append({speakers[i]});
        fullWeights.append((baseValue + (i < remainder ? 1 : 0)) / 100.0);
    }
    data.fixedWeights = explicitWeightsFromFullWeights(fullWeights);
    return normalizeSpeakerMixData(data);
}

void SpeakerMixDialog::onPresetSelected(const int index) {
    if (!m_cbPresets)
        return;

    const QString presetId = m_cbPresets->itemData(index).toString();
    if (presetId.isEmpty()) {
        m_currentPresetId.clear();
        applySpeakerMixDataToUi(equalWeightMixData());
        return;
    }

    if (applyPresetToUi(presetId, true))
        m_currentPresetId = presetId;
}

void SpeakerMixDialog::onSaveAsPreset() {
    QString defaultName;
    if (!m_currentPresetId.isEmpty()) {
        if (const auto preset = SpeakerMixPresetStore::findPreset(m_currentPresetId))
            defaultName = preset->name;
    }

    bool ok = false;
    const QString name =
        QInputDialog::getText(this, {}, tr("Preset name"), QLineEdit::Normal, defaultName, &ok)
            .trimmed();
    if (!ok || name.isEmpty())
        return;

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

    // If a preset with this name already exists, overwrite it (carry over its id)
    if (const auto existing = SpeakerMixPresetStore::findPresetByName(m_singerInfo, name))
        preset.id = existing->id;

    if (const auto saved = SpeakerMixPresetStore::savePreset(preset)) {
        reloadPresetCombo(saved->id);
    } else {
        Toast::show(tr("Failed to save preset"));
    }
}

void SpeakerMixDialog::onDeletePreset() {
    if (m_currentPresetId.isEmpty())
        return;
    if (QMessageBox::question(this, {}, tr("Delete this preset?")) != QMessageBox::Yes)
        return;
    SpeakerMixPresetStore::deletePreset(m_currentPresetId);
    reloadPresetCombo();
    applySpeakerMixDataToUi(equalWeightMixData());
}

void SpeakerMixDialog::onInitializePreset() {
    if (QMessageBox::question(
            this, {}, tr("Reset to init preset?\nAll current mix settings will be lost.")) !=
        QMessageBox::Yes)
        return;
    m_currentPresetId.clear();
    if (m_cbPresets)
        m_cbPresets->setCurrentIndex(0);
    applySpeakerMixDataToUi(equalWeightMixData());
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
