#include "TrackAreaView.h"

#include <lite/GUI/Controls/DividerLine.h>

#include <QVBoxLayout>

TrackAreaView::TrackAreaView(QWidget *contentWidget, QWidget *parent) : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);

    m_topDivider = new DividerLine(Qt::Horizontal, this);
    m_topDivider->setObjectName("trackAreaDivider");
    m_topDivider->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_topDivider->setLineMargin(0);
    m_topDivider->setFixedHeight(1);

    setContentWidget(contentWidget);
}

void TrackAreaView::setContentWidget(QWidget *contentWidget) {
    if (!contentWidget || contentWidget == m_contentWidget)
        return;

    if (m_contentWidget) {
        m_layout->removeWidget(m_contentWidget);
        m_contentWidget->hide();
    }

    m_contentWidget = contentWidget;
    m_layout->addWidget(m_contentWidget);
    setSizePolicy(m_contentWidget->sizePolicy());
    m_contentWidget->show();
    m_topDivider->raise();
}

void TrackAreaView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_topDivider->setGeometry(0, 0, width(), 1);
    m_topDivider->raise();
}
