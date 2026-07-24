//
// Created by fluty on 2026/5/4.
//

#include "SpeakerMixToolBarView.h"

#include "UI/Controls/ColorDot.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QLabel>

SpeakerMixToolBarView::SpeakerMixToolBarView(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout;

    m_btnBypassToggle = new Button(tr("Bypass"));
    m_btnBypassToggle->setObjectName("btnDynamicMixBypassToggle");
    m_btnBypassToggle->setCheckable(true);
    m_btnStop = new Button(tr("Stop Dynamic..."));
    m_btnStop->setObjectName("btnStopDynamicMix");

    m_btnPrev = new Button(QStringLiteral("\u25C0"));
    m_btnPrev->setObjectName("btnPrevKeyframe");
    m_btnNext = new Button(QStringLiteral("\u25B6"));
    m_btnNext->setObjectName("btnNextKeyframe");

    m_btnBypassToggle->setFixedHeight(ClipEditorGlobal::paramEditorToolControlHeight);
    m_btnStop->setFixedHeight(ClipEditorGlobal::paramEditorToolControlHeight);
    m_btnPrev->setFixedSize(ClipEditorGlobal::paramEditorToolControlHeight,
                            ClipEditorGlobal::paramEditorToolControlHeight);
    m_btnNext->setFixedSize(ClipEditorGlobal::paramEditorToolControlHeight,
                            ClipEditorGlobal::paramEditorToolControlHeight);

    layout->addWidget(m_btnBypassToggle);
    layout->addWidget(m_btnStop);
    layout->addSpacing(8);
    layout->addWidget(m_btnPrev);
    layout->addWidget(m_btnNext);
    layout->addSpacing(8);

    m_speakerContainer = new QWidget;
    auto *speakerLayout = new QHBoxLayout;
    speakerLayout->setContentsMargins(0, 0, 0, 0);
    speakerLayout->setSpacing(8);
    m_speakerContainer->setLayout(speakerLayout);
    layout->addWidget(m_speakerContainer);

    layout->setSpacing(4);
    layout->setContentsMargins(0, 0, 0, 0);

    setLayout(layout);

    connect(m_btnBypassToggle, &Button::clicked, this, [this](const bool bypassed) {
        if (bypassed)
            emit bypassDynamicMix();
        else
            emit resumeDynamicMix();
    });
    connect(m_btnStop, &Button::clicked, this, &SpeakerMixToolBarView::stopDynamicMix);
    connect(m_btnPrev, &Button::clicked, this, &SpeakerMixToolBarView::previousKeyframe);
    connect(m_btnNext, &Button::clicked, this, &SpeakerMixToolBarView::nextKeyframe);

    setDynamicState(SpeakerMixDynamicUiState::Unavailable);
}

void SpeakerMixToolBarView::setSpeakers(const QStringList &names, const QList<QColor> &colors) {
    if (m_speakerNames == names && m_speakerColors == colors)
        return;

    m_speakerNames = names;
    m_speakerColors = colors;

    auto *speakerLayout = m_speakerContainer->layout();
    QLayoutItem *item;
    while ((item = speakerLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (int i = 0; i < names.size() && i < colors.size(); i++) {
        auto *dot = new ColorDot(colors[i]);

        auto *nameLabel = new QLabel(names[i]);

        auto *itemLayout = new QHBoxLayout;
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(4);
        itemLayout->addWidget(dot);
        itemLayout->addWidget(nameLabel);

        auto *itemWidget = new QWidget;
        itemWidget->setLayout(itemLayout);
        speakerLayout->addWidget(itemWidget);
    }
}

void SpeakerMixToolBarView::setDynamicState(const SpeakerMixDynamicUiState state) {
    m_dynamicState = state;
    const bool active = state == SpeakerMixDynamicUiState::Active;
    const bool bypassed = state == SpeakerMixDynamicUiState::Bypassed;
    const bool hasDynamic = active || bypassed;

    updateBypassButton();
    m_btnStop->setVisible(hasDynamic);
    m_btnPrev->setVisible(hasDynamic);
    m_btnNext->setVisible(hasDynamic);
    m_btnPrev->setEnabled(hasDynamic);
    m_btnNext->setEnabled(hasDynamic);
}

void SpeakerMixToolBarView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        updateBypassButton();
        m_btnStop->setText(tr("Stop Dynamic..."));
    }
}

void SpeakerMixToolBarView::updateBypassButton() {
    const bool bypassed = m_dynamicState == SpeakerMixDynamicUiState::Bypassed;
    const bool hasDynamic = m_dynamicState == SpeakerMixDynamicUiState::Active || bypassed;
    m_btnBypassToggle->setVisible(hasDynamic);
    m_btnBypassToggle->setChecked(bypassed);
    m_btnBypassToggle->setText(bypassed ? tr("Cancel Bypass") : tr("Bypass"));
}
