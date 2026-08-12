#include "PlaybackArbiter.h"

#include "Controller/PlaybackController.h"
#include "DocumentSession.h"

PlaybackArbiter::PlaybackArbiter(QObject *parent) : QObject(parent) {
}

void PlaybackArbiter::addSession(DocumentSession *session) {
    if (!session || m_connections.contains(session))
        return;

    m_connections.insert(
        session, connect(session->playback(), &PlaybackController::playbackStatusChanged, this,
                         [this, session](const PlaybackGlobal::PlaybackStatus status) {
                             if (status != PlaybackGlobal::Playing)
                                 return;
                             for (auto it = m_connections.cbegin(); it != m_connections.cend();
                                  ++it) {
                                 auto *other = it.key();
                                 if (other == session ||
                                     other->playback()->playbackStatus() == PlaybackGlobal::Stopped)
                                     continue;
                                 other->activate();
                                 other->playback()->stop();
                             }
                             session->activate();
                         }));
}

void PlaybackArbiter::removeSession(DocumentSession *session) {
    const auto connection = m_connections.take(session);
    if (connection)
        disconnect(connection);
}
