#include "RenderUpdateScheduler.h"

#include <QTimer>

RenderUpdateScheduler::RenderUpdateScheduler(QObject *parent) : QObject(parent) {
}

void RenderUpdateScheduler::request(const EditorDirtyDomains domains) {
    if (domains == EditorDirtyDomain::None)
        return;
    m_pending |= domains;
    if (m_flushScheduled)
        return;
    m_flushScheduled = true;
    QTimer::singleShot(0, this, &RenderUpdateScheduler::flush);
}

EditorDirtyDomains RenderUpdateScheduler::pendingDomains() const {
    return m_pending;
}

void RenderUpdateScheduler::flush() {
    m_flushScheduled = false;
    const auto domains = m_pending;
    m_pending = EditorDirtyDomain::None;
    if (domains != EditorDirtyDomain::None)
        emit updateRequested(domains);
}
