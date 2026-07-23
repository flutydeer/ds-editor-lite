#include "Model/Utils/DiffscopeAudioWorkspace.h"

#include "Model/AppModel/AudioClip.h"

#include <QDir>
#include <QFileInfo>

namespace DiffscopeAudioWorkspace {

    static constexpr auto kDiffscopeKey = "diffscope";
    static constexpr auto kAudioKey = "audio";

    AudioPathInfo read(const QMap<QString, QJsonObject> &workspace) {
        AudioPathInfo info;
        const auto audioObj = workspace.value(kDiffscopeKey).value(kAudioKey).toObject();
        info.relativeDir = audioObj.value("relativeDir").toString();
        info.sha512 = audioObj.value("sha512").toString();
        return info;
    }

    void write(QMap<QString, QJsonObject> &workspace, const AudioPathInfo &info,
               const QString &absolutePath) {
        if (absolutePath.isEmpty())
            return;
        const QFileInfo fileInfo(absolutePath);
        auto diffscopeObj = workspace.value(kDiffscopeKey);
        auto audioObj = diffscopeObj.value(kAudioKey).toObject();
        audioObj["absoluteDir"] = fileInfo.absolutePath();
        audioObj["relativeDir"] = info.relativeDir;
        audioObj["fileName"] = fileInfo.fileName();
        audioObj["sha512"] = info.sha512;
        diffscopeObj[kAudioKey] = audioObj;
        workspace[kDiffscopeKey] = diffscopeObj;
    }

    QString relativeDirFor(const QString &audioFilePath, const QString &projectFilePath) {
        if (audioFilePath.isEmpty() || projectFilePath.isEmpty())
            return {};
        const QDir projectDir = QFileInfo(projectFilePath).absoluteDir();
        const auto audioDir = QFileInfo(audioFilePath).absolutePath();
        const auto relative = projectDir.relativeFilePath(audioDir);
        if (relative.startsWith(QStringLiteral("..")) || QDir::isAbsolutePath(relative))
            return {};
        return relative;
    }

} // namespace DiffscopeAudioWorkspace
