#ifndef OPENDSPXPROJECTTASK_H
#define OPENDSPXPROJECTTASK_H

#include "Modules/ProjectConverters/DspxProjectParser.h"
#include "Modules/Task/Task.h"

class OpenDspxProjectTask final : public Task {
public:
    explicit OpenDspxProjectTask(QString filePath, quint64 requestId);

    [[nodiscard]] const QString &filePath() const;
    [[nodiscard]] quint64 requestId() const;
    [[nodiscard]] qint64 fileSize() const;
    [[nodiscard]] qint64 readElapsedMs() const;
    [[nodiscard]] qint64 parseElapsedMs() const;
    DspxParseResult takeResult();

private:
    void runTask() override;

    QString m_filePath;
    quint64 m_requestId = 0;
    qint64 m_fileSize = 0;
    qint64 m_readElapsedMs = 0;
    qint64 m_parseElapsedMs = 0;
    DspxParseResult m_result;
};

#endif // OPENDSPXPROJECTTASK_H
