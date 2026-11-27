#ifndef DS_EDITOR_LITE_EXTERNALFILECLASSIFIER_H
#define DS_EDITOR_LITE_EXTERNALFILECLASSIFIER_H

#include "PreparedImportItem.h"

#include <QString>
#include <QStringList>
#include <QUrl>

// Classifies external files (dropped or picked via dialogs) into project /
// MIDI / audio / unsupported without touching any model state.
class ExternalFileClassifier {
public:
    struct Result {
        ExternalFileKind kind = ExternalFileKind::Unsupported;
        QString reason;
    };

    static Result classify(const QString &path);

    struct DroppedFiles {
        QStringList projects;
        QStringList imports;

        bool projectsOnly() const { return !projects.isEmpty() && imports.isEmpty(); }
    };
    static DroppedFiles classifyDrop(const QList<QUrl> &urls);

private:
    static bool isAudioExtension(const QString &extension);
};

#endif // DS_EDITOR_LITE_EXTERNALFILECLASSIFIER_H
