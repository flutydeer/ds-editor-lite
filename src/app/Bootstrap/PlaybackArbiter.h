#ifndef PLAYBACKARBITER_H
#define PLAYBACKARBITER_H

#include <QHash>
#include <QObject>

class DocumentSession;
class PlaybackController;

class PlaybackArbiter final : public QObject {
public:
    explicit PlaybackArbiter(QObject *parent = nullptr);

    void addSession(DocumentSession *session);
    void removeSession(DocumentSession *session);

private:
    QHash<DocumentSession *, QMetaObject::Connection> m_connections;
};

#endif // PLAYBACKARBITER_H
