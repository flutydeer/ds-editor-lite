#ifndef LEGACYMIDILOADSESSION_H
#define LEGACYMIDILOADSESSION_H

#include "IProjectLoadSession.h"

class LegacyMidiLoadSession final : public IProjectLoadSession {
    Q_OBJECT

public:
    LegacyMidiLoadSession(QString filePath, ProjectLoadPurpose purpose, quint64 requestId,
                          QObject *parent = nullptr);

    void start() override;
    void cancel() override;
    PreparedProject takeResult() override;
    [[nodiscard]] quint64 requestId() const override;

private:
    QString m_filePath;
    ProjectLoadPurpose m_purpose = ProjectLoadPurpose::Open;
    quint64 m_requestId = 0;
    PreparedProject m_result;
    bool m_started = false;
    bool m_terminal = false;
};

#endif // LEGACYMIDILOADSESSION_H
