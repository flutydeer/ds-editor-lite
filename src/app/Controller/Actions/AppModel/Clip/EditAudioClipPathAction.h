#ifndef EDITAUDIOCLIPPATHACTION_H
#define EDITAUDIOCLIPPATHACTION_H

#include "Model/AppModel/AudioClip.h"
#include "Modules/History/IAction.h"

#include <QJsonObject>

// Relocates/replaces the file of an audio clip (relink).
// Restores the {path, pathInfo, formatData workspace} triple;
// path changes drive playback reload and waveform re-decoding via propertyChanged.
// Manual relinks MUST be undoable: the user-picked file is unverified and may have
// different content (this is also the "replace source" feature). In contrast,
// sha512-verified auto/cascade relocation bypasses this action on purpose — see
// AudioDecodingController::resolveMissingClipsNear for the undo-boundary rationale
class EditAudioClipPathAction : public IAction {
public:
    static EditAudioClipPathAction *build(AudioClip *clip, const QString &newPath,
                                          const AudioPathInfo &newPathInfo,
                                          const QJsonObject &newFormatData);
    void execute() override;
    void undo() override;

private:
    void apply(const QString &path, const AudioPathInfo &pathInfo,
               const QJsonObject &formatData) const;

    AudioClip *m_clip = nullptr;
    QString m_oldPath;
    QString m_newPath;
    AudioPathInfo m_oldPathInfo;
    AudioPathInfo m_newPathInfo;
    QJsonObject m_oldFormatData;
    QJsonObject m_newFormatData;
    AudioClip::PathStatus m_oldStatus = AudioClip::PathStatus::Normal;
};

#endif // EDITAUDIOCLIPPATHACTION_H
