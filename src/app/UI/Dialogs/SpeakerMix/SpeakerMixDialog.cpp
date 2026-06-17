#include "SpeakerMixDialog.h"
#include "SpeakerMixBar.h"
#include "SpeakerMixList.h"
#include "UI/Controls/FlowLayout.h"
#include "UI/Controls/TagButton.h"

#include <QSignalBlocker>
#include <QVBoxLayout>

SpeakerMixDialog::SpeakerMixDialog(QWidget *parent) : Dialog(parent) {
    setWindowTitle("Speaker Mix");
    setTitle("Speaker Mix");
    setMinimumWidth(480);
    setMinimumHeight(312);

    const QStringList speakerTypes = {"朱", "樱", "琪", "梨", "珏"};

    m_mixList = new SpeakerMixList("test_package", speakerTypes, this);

    const auto tagContainer = new QWidget(this);
    tagContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    const auto tagLayout = new FlowLayout(tagContainer, 0, 6, 6);

    const auto &colors = SpeakerMixList::defaultColors();
    for (int i = 0; i < speakerTypes.size(); ++i) {
        const QString &name = speakerTypes[i];

        const auto btn = new TagButton(name, tagContainer);
        btn->setChecked(true);

        btn->setProperty("speakerName", name);
        connect(btn, &TagButton::toggled, this, &SpeakerMixDialog::onTagToggled);
        tagLayout->addWidget(btn);
        m_tagButtons[name] = btn;

        m_mixList->addSpeaker(name);
    }

    connect(m_mixList, &SpeakerMixList::speakerChanged, this,
            &SpeakerMixDialog::onSpeakerChangedInList);

    const auto layout = new QVBoxLayout(body());
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(tagContainer);
    layout->addWidget(m_mixList);
    layout->addWidget(m_mixList->getMixBar());
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
