#include "SpeakerMixDialog.h"
#include "SpeakerMixBar.h"
#include "SpeakerMixList.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/FlowLayout.h"
#include "UI/Controls/TagButton.h"

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

    m_mixList = new SpeakerMixList(m_singerInfo.name(), speakerTypes, this);
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
        SpeakerMixData data;
        data.mode = SingerSourceMode::FixedMix;
        const auto labels = m_mixList->getLabels();
        for (const auto &label : labels) {
            auto speaker = speakerById(label);
            if (!speaker.isEmpty())
                data.sources.append({speaker});
        }
        data.fixedWeights = explicitWeightsFromFullWeights(currentFullWeights());
        m_result = normalizeSpeakerMixData(data);
        accept();
    });
    connect(cancelButton(), &Button::clicked, this, &Dialog::reject);

    setupInitialSources(mixData);

    const auto layout = new QVBoxLayout(body());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(tagContainer);
    layout->addWidget(m_mixList);
    layout->addWidget(m_mixList->getMixBar());
}

SpeakerMixData SpeakerMixDialog::speakerMixData() const {
    return m_result;
}

void SpeakerMixDialog::setupInitialSources(const SpeakerMixData &mixData) {
    const auto normalizedMix = normalizeSpeakerMixData(mixData);
    QVector<double> fullWeights;

    if (normalizedMix.mode == SingerSourceMode::FixedMix) {
        for (const auto &source : normalizedMix.sources) {
            const auto id = source.speaker.id();
            if (!m_tagButtons.contains(id))
                continue;
            m_mixList->addSpeaker(id);
        }
        fullWeights = fullWeightsFromExplicitWeights(normalizedMix.fixedWeights);
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
