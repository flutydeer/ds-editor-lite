//
// Created by fluty on 2026/5/4.
//

#include "SpeakerMixToolBarView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

SpeakerMixToolBarView::SpeakerMixToolBarView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    auto *layout = new QHBoxLayout;

    m_btnPrev = new Button(QStringLiteral("\u25C0"));
    m_btnPrev->setFixedWidth(28);
    m_btnNext = new Button(QStringLiteral("\u25B6"));
    m_btnNext->setFixedWidth(28);

    layout->addWidget(m_btnPrev);
    layout->addWidget(m_btnNext);
    layout->addSpacing(8);

    m_speakerContainer = new QWidget;
    auto *speakerLayout = new QHBoxLayout;
    speakerLayout->setContentsMargins(0, 0, 0, 0);
    speakerLayout->setSpacing(8);
    m_speakerContainer->setLayout(speakerLayout);
    layout->addWidget(m_speakerContainer);

    layout->addStretch();
    layout->setSpacing(4);
    layout->setContentsMargins(8, 4, 4, 4);

    setLayout(layout);
    setFixedHeight(36);

    connect(m_btnPrev, &Button::clicked, this, &SpeakerMixToolBarView::previousKeyframe);
    connect(m_btnNext, &Button::clicked, this, &SpeakerMixToolBarView::nextKeyframe);
}

void SpeakerMixToolBarView::setSpeakers(const QStringList &names, const QList<QColor> &colors) {
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
