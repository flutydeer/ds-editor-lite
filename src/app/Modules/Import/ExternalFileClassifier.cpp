#include "ExternalFileClassifier.h"

#include <QFileInfo>
#include <QUrl>

#include <TalcsFormat/FormatManager.h>

#include "Modules/Audio/AudioContext.h"

ExternalFileClassifier::Result ExternalFileClassifier::classify(const QString &path) {
    const QUrl url = QUrl::fromUserInput(path);
    if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")
                          || url.scheme() == QLatin1String("ftp"))) {
        return {ExternalFileKind::Unsupported, QStringLiteral("Remote URLs are not supported")};
    }
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return {ExternalFileKind::Unsupported, QStringLiteral("File does not exist or is not a file")};
    }
    const auto suffix = info.suffix().toLower();
    if (suffix == QLatin1String("dspx"))
        return {ExternalFileKind::Project, {}};
    if (suffix == QLatin1String("mid") || suffix == QLatin1String("midi"))
        return {ExternalFileKind::Midi, {}};
    if (isAudioExtension(suffix))
        return {ExternalFileKind::Audio, {}};
    return {ExternalFileKind::Unsupported, QStringLiteral("Unsupported file type")};
}

bool ExternalFileClassifier::isAudioExtension(const QString &extension) {
    const auto hints = audioContext->formatManager()->extensionHints();
    for (const auto &hint : hints) {
        const auto parts = hint.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (auto part : parts) {
            if (part.startsWith(QLatin1Char('*')))
                part.remove(0, 1);
            if (part.compare(extension, Qt::CaseInsensitive) == 0)
                return true;
        }
    }
    return false;
}
