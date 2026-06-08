//
// Created by FlutyDeer on 2026/6/8.
//

#include "EditSessionManager.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClip.h"

#include <QDebug>

namespace {
    template <typename T>
    void appendUnique(QList<T> &list, const T &value) {
        if (!list.contains(value))
            list.append(value);
    }

    template <typename T>
    void appendUnique(QList<T> &list, const QList<T> &values) {
        for (const auto &value : values)
            appendUnique(list, value);
    }
}

EditSessionManager::EditSessionManager(QObject *parent) : QObject(parent) {
}

EditSessionManager::~EditSessionManager() = default;

bool EditSessionManager::hasActiveTransaction() const {
    return m_hasActiveSession;
}

EditSession EditSessionManager::activeSession() const {
    return m_activeSession;
}

quint64 EditSessionManager::beginTransaction(const EditSession &session) {
    if (m_hasActiveSession) {
        qWarning() << "Replacing active edit session" << "sessionId:" << m_activeSession.sessionId
                   << "domain:" << static_cast<int>(m_activeSession.domain);
        endActiveTransaction(EditSessionEndReason::Cancel);
    }

    m_activeSession = normalizeSession(session);
    m_activeSession.sessionId = m_nextSessionId++;
    m_hasActiveSession = true;

    qDebug() << "Edit session began" << "sessionId:" << m_activeSession.sessionId
             << "domain:" << static_cast<int>(m_activeSession.domain)
             << "clipId:" << m_activeSession.clipId << "clipIds:" << m_activeSession.clipIds
             << "noteIds:" << m_activeSession.noteIds << "pieceIds:" << m_activeSession.pieceIds
             << "wholeClipScope:" << m_activeSession.wholeClipScope
             << "baseRevision:" << m_activeSession.baseRevision;
    emit editSessionBegan(m_activeSession);
    return m_activeSession.sessionId;
}

quint64 EditSessionManager::beginTransaction(const AppStatus::EditObjectType domain,
                                             const int clipId, const QList<int> &clipIds,
                                             const QList<int> &noteIds, const QList<int> &pieceIds,
                                             const QList<ParamInfo::Name> &params,
                                             const bool wholeClipScope) {
    EditSession session;
    session.domain = domain;
    session.clipId = clipId;
    session.clipIds = clipIds;
    session.noteIds = noteIds;
    session.pieceIds = pieceIds;
    session.params = params;
    session.wholeClipScope = wholeClipScope;
    return beginTransaction(session);
}

void EditSessionManager::endTransaction(const quint64 sessionId,
                                        const EditSessionEndReason reason) {
    if (!m_hasActiveSession)
        return;
    if (m_activeSession.sessionId != sessionId) {
        qWarning() << "Ignored edit session end with mismatched id"
                   << "activeSessionId:" << m_activeSession.sessionId
                   << "requestedSessionId:" << sessionId;
        return;
    }

    const auto endedSession = m_activeSession;
    m_activeSession = {};
    m_hasActiveSession = false;

    qDebug() << "Edit session ended" << "sessionId:" << endedSession.sessionId
             << "domain:" << static_cast<int>(endedSession.domain)
             << "reason:" << static_cast<int>(reason);
    emit editSessionEnded(endedSession, reason);
}

void EditSessionManager::endActiveTransaction(const EditSessionEndReason reason) {
    if (!m_hasActiveSession)
        return;
    endTransaction(m_activeSession.sessionId, reason);
}

void EditSessionManager::addNoteIds(const QList<int> &noteIds) {
    if (!m_hasActiveSession)
        return;
    appendUnique(m_activeSession.noteIds, noteIds);
}

void EditSessionManager::clear() {
    m_activeSession = {};
    m_hasActiveSession = false;
}

EditSession EditSessionManager::normalizeSession(EditSession session) const {
    if (session.clipId < 0 && !session.clipIds.isEmpty())
        session.clipId = session.clipIds.first();
    if (session.clipId >= 0)
        appendUnique(session.clipIds, session.clipId);

    if (session.baseRevision == 0 && session.clipId >= 0) {
        if (const auto clipObject = appModel->findClipById(session.clipId)) {
            if (clipObject->clipType() == IClip::Singing)
                session.baseRevision = static_cast<SingingClip *>(clipObject)->inferenceRevision();
        }
    }
    return session;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(EditSessionManager)
