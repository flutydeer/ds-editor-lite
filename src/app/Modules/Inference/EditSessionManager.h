//
// Created by FlutyDeer on 2026/6/8.
//

#ifndef DS_EDITOR_LITE_EDITSESSIONMANAGER_H
#define DS_EDITOR_LITE_EDITSESSIONMANAGER_H

#define editSessionManager EditSessionManager::instance()

#include "Model/AppModel/Params.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/Singleton.h"

#include <QObject>

enum class EditSessionEndReason {
    Commit,
    Discard,
    Cancel,
    Unknown,
};

struct EditSession {
    quint64 sessionId = 0;
    AppStatus::EditObjectType domain = AppStatus::EditObjectType::None;
    int clipId = -1;
    QList<int> clipIds;
    QList<int> noteIds;
    QList<int> pieceIds;
    QList<ParamInfo::Name> params;
    bool wholeClipScope = false;
    quint64 baseRevision = 0;
};

class EditSessionManager final : public QObject {
    Q_OBJECT

private:
    explicit EditSessionManager(QObject *parent = nullptr);
    ~EditSessionManager() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(EditSessionManager)
    Q_DISABLE_COPY_MOVE(EditSessionManager)

    [[nodiscard]] bool hasActiveTransaction() const;
    [[nodiscard]] EditSession activeSession() const;

    quint64 beginTransaction(const EditSession &session);
    quint64 beginTransaction(AppStatus::EditObjectType domain, int clipId,
                             const QList<int> &clipIds = {}, const QList<int> &noteIds = {},
                             const QList<int> &pieceIds = {},
                             const QList<ParamInfo::Name> &params = {},
                             bool wholeClipScope = false);
    void endTransaction(quint64 sessionId, EditSessionEndReason reason);
    void endActiveTransaction(EditSessionEndReason reason);
    void addNoteIds(const QList<int> &noteIds);
    void clear();

signals:
    void editSessionBegan(const EditSession &session);
    void editSessionEnded(const EditSession &session, EditSessionEndReason reason);

private:
    [[nodiscard]] EditSession normalizeSession(EditSession session) const;

    bool m_hasActiveSession = false;
    quint64 m_nextSessionId = 1;
    EditSession m_activeSession;
};

#endif // DS_EDITOR_LITE_EDITSESSIONMANAGER_H
