#ifndef RENDERUPDATESCHEDULER_H
#define RENDERUPDATESCHEDULER_H

#include "EditorCanvasTypes.h"

#include <QObject>

class RenderUpdateScheduler final : public QObject {
    Q_OBJECT

public:
    explicit RenderUpdateScheduler(QObject *parent = nullptr);

    void request(EditorDirtyDomains domains);
    [[nodiscard]] EditorDirtyDomains pendingDomains() const;

signals:
    void updateRequested(EditorDirtyDomains domains);

private:
    void flush();

    EditorDirtyDomains m_pending = EditorDirtyDomain::None;
    bool m_flushScheduled = false;
};

#endif // RENDERUPDATESCHEDULER_H
