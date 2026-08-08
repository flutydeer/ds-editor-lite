#ifndef DS_EDITOR_LITE_LIBRESVIPCONVERTTASK_H
#define DS_EDITOR_LITE_LIBRESVIPCONVERTTASK_H

#include <lite/ProjectConverters/DspxProjectParser.h>
#include <lite/Tasking/Task.h>

// Runs `libresvip-cli proj convert <input> <output.dspx>` in a background
// task and parses the zstd-compressed DSPX data it writes. The CLI asks
// format-specific questions on stdin; feeding excess empty lines accepts
// every default answer, so no per-format prompt handling is needed.
class LibreSVIPConvertTask final : public Task {
public:
    LibreSVIPConvertTask(QString executablePath, QString inputPath);

    [[nodiscard]] const QString &executablePath() const;
    [[nodiscard]] const QString &inputPath() const;
    [[nodiscard]] const QString &outputPath() const;
    DspxParseResult takeResult();

private:
    void runTask() override;

    QString m_executablePath;
    QString m_inputPath;
    QString m_outputPath;
    DspxParseResult m_result;
};

#endif // DS_EDITOR_LITE_LIBRESVIPCONVERTTASK_H
