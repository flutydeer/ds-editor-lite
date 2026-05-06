//
// Created by fluty on 24-9-16.
//

#include "ParamEditorToolBarView.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/ComboBox.h"
#include "Utils/ParamUtils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

ParamEditorToolBarView::ParamEditorToolBarView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    lbForegroundParam = new QLabel(tr("Foreground:"));
    lbForegroundParam->setObjectName("lbForegroundParam");

    cbForegroundParam = new ComboBox(true);
    cbForegroundParam->setObjectName("cbForegroundParam");
    cbForegroundParam->addItems(paramUtils->names());
    cbForegroundParam->removeItem(0); // Remove pitch
    cbForegroundParam->addItem(tr("Speaker Mix"));

    const auto btnSwap = new Button(tr("Swap"));
    btnSwap->setObjectName("btnSwap");

    lbBackgroundParam = new QLabel(tr("Background:"));
    lbBackgroundParam->setObjectName("lbBackgroundParam");

    cbBackgroundParam = new ComboBox(true);
    cbBackgroundParam->setObjectName("cbBackgroundParam");
    cbBackgroundParam->addItems(paramUtils->names());
    cbBackgroundParam->removeItem(0); // Remove pitch

    // Speaker mix section (hidden by default)
    m_speakerMixSection = new QWidget;
    m_speakerMixSection->setObjectName("speakerMixSection");
    m_speakerMixSection->setAttribute(Qt::WA_StyledBackground);
    
    auto *mixLayout = new QHBoxLayout;
    mixLayout->setContentsMargins(0, 0, 0, 0);
    mixLayout->setSpacing(4);

    auto *btnPrev = new Button(QStringLiteral("◀"));
    btnPrev->setObjectName("btnPrevKeyframe");
    
    auto *btnNext = new Button(QStringLiteral("▶"));
    btnNext->setObjectName("btnNextKeyframe");
    mixLayout->addWidget(btnPrev);
    mixLayout->addWidget(btnNext);
    mixLayout->addSpacing(8);

    m_speakerContainer = new QWidget;
    auto *speakerLayout = new QHBoxLayout;
    speakerLayout->setContentsMargins(0, 0, 0, 0);
    speakerLayout->setSpacing(8);
    m_speakerContainer->setLayout(speakerLayout);
    mixLayout->addWidget(m_speakerContainer);

    m_speakerMixSection->setLayout(mixLayout);
    m_speakerMixSection->setVisible(false);

    connect(btnPrev, &Button::clicked, this, &ParamEditorToolBarView::previousKeyframe);
    connect(btnNext, &Button::clicked, this, &ParamEditorToolBarView::nextKeyframe);

    const auto layout = new QHBoxLayout();
    layout->addSpacing(64);
    layout->addWidget(lbForegroundParam);
    layout->addWidget(cbForegroundParam);
    layout->addWidget(btnSwap);
    layout->addWidget(lbBackgroundParam);
    layout->addWidget(cbBackgroundParam);
    layout->addWidget(m_speakerMixSection);
    layout->addStretch();
    layout->setSpacing(4);
    layout->setContentsMargins(8, 4, 4, 4);

    setLayout(layout);
    // setFixedHeight(32);

    connect(cbForegroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onForegroundSelectionChanged);
    connect(cbBackgroundParam, &ComboBox::currentIndexChanged, this,
            &ParamEditorToolBarView::onBackgroundSelectionChanged);
    connect(btnSwap, &Button::clicked, this, &ParamEditorToolBarView::onSwap);

    cbForegroundParam->setCurrentIndex(appOptions->general()->defaultForegroundParam - 1);
    cbBackgroundParam->setCurrentIndex(appOptions->general()->defaultBackgroundParam - 1);
}

void ParamEditorToolBarView::setSpeakerMixMode(bool on) {
    m_speakerMixSection->setVisible(on);
}

void ParamEditorToolBarView::setSpeakers(const QStringList &names, const QList<QColor> &colors) {
    auto *speakerLayout = m_speakerContainer->layout();
    QLayoutItem *item;
    while ((item = speakerLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (int i = 0; i < names.size() && i < colors.size(); i++) {
        auto *dot = new QLabel;
        const int size = 10;
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors[i]);
        painter.drawEllipse(0, 0, size, size);
        painter.end();
        dot->setPixmap(pixmap);
        dot->setFixedSize(size, size);

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

void ParamEditorToolBarView::onForegroundSelectionChanged(const int index) {
    emit foregroundChanged(static_cast<ParamInfo::Name>(index + 1));
}

void ParamEditorToolBarView::onBackgroundSelectionChanged(const int index) {
    emit backgroundChanged(static_cast<ParamInfo::Name>(index + 1));
}

void ParamEditorToolBarView::onSwap() const {
    const int fgIndex = cbForegroundParam->currentIndex();
    const int speakerMixIndex = cbForegroundParam->count() - 1;
    if (fgIndex == speakerMixIndex)
        return;
    const int temp = fgIndex;
    cbForegroundParam->setCurrentIndex(cbBackgroundParam->currentIndex());
    cbBackgroundParam->setCurrentIndex(temp);
}
