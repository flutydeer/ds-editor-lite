//
// Created by fluty on 2026/5/26.
//

#include "ControlGroup.h"

#include <QChildEvent>
#include <QLayout>
#include <QStyle>
#include <QTimer>

ControlGroup::ControlGroup(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
}

void ControlGroup::childEvent(QChildEvent *event) {
    QWidget::childEvent(event);
    if (event->child()->isWidgetType()) {
        if (event->added())
            event->child()->installEventFilter(this);
        scheduleUpdate();
    }
}

bool ControlGroup::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::Show || event->type() == QEvent::Hide)
        scheduleUpdate();
    return QWidget::eventFilter(obj, event);
}

void ControlGroup::scheduleUpdate() {
    if (m_updatePending)
        return;
    m_updatePending = true;
    QTimer::singleShot(0, this, &ControlGroup::updateGroupPositions);
}

void ControlGroup::updateGroupPositions() {
    m_updatePending = false;
    auto lay = layout();
    if (!lay)
        return;

    QList<QWidget *> visible;
    for (int i = 0; i < lay->count(); i++) {
        if (auto w = lay->itemAt(i)->widget(); w && w->isVisible())
            visible.append(w);
    }

    for (int i = 0; i < visible.size(); i++) {
        auto w = visible[i];
        QString pos;
        if (visible.size() == 1)
            pos = "only";
        else if (i == 0)
            pos = "first";
        else if (i == visible.size() - 1)
            pos = "last";
        else
            pos = "middle";

        if (w->property("groupPos") != pos) {
            w->setProperty("groupPos", pos);
            w->style()->unpolish(w);
            w->style()->polish(w);
        }
    }
}
