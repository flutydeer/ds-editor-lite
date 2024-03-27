#include "LyricWrapWidget.h"

namespace LyricWrap {
    LyricWrapWidget::LyricWrapWidget(QWidget *parent)
        : QScrollArea(parent), m_view(new LyricWrapView()) {
        this->setBackgroundRole(QPalette::Dark);

        this->setWidgetResizable(true);
        this->setWidget(m_view);
    }

    void LyricWrapWidget::appendList(const QList<LangNote *> &noteList) const {
        m_view->appendList(noteList);
    }

} // LyricWrap