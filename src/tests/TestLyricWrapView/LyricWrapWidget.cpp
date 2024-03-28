#include "LyricWrapWidget.h"

#include <QVBoxLayout>

namespace LyricWrap {
    LyricWrapWidget::LyricWrapWidget(QWidget *parent)
        : QScrollArea(parent), m_view(new LyricWrapView()) {
        this->setBackgroundRole(QPalette::Dark);

        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        this->setWidgetResizable(true);
        this->setWidget(m_view);
        m_view->setHeight(height());
    }

    void LyricWrapWidget::resizeEvent(QResizeEvent *event) {
        QScrollArea::resizeEvent(event);
        m_view->setHeight(height());
    }

    void LyricWrapWidget::appendList(const QList<LangNote *> &noteList) const {
        m_view->appendList(noteList);
    }

} // LyricWrap