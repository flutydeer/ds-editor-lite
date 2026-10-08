#include "EditAudioClipPathAction.h"

static constexpr auto kFormatDataKey = "diffscope.audio.formatData";

EditAudioClipPathAction *EditAudioClipPathAction::build(AudioClip *clip, const QString &newPath,
                                                        const AudioPathInfo &newPathInfo,
                                                        const QJsonObject &newFormatData) {
    auto a = new EditAudioClipPathAction;
    a->m_clip = clip;
    a->m_oldPath = clip->path();
    a->m_newPath = newPath;
    a->m_oldPathInfo = clip->pathInfo();
    a->m_newPathInfo = newPathInfo;
    a->m_oldFormatData = clip->workspace().value(kFormatDataKey);
    a->m_hadFormatData = clip->workspace().contains(kFormatDataKey);
    a->m_newFormatData = newFormatData;
    a->m_oldStatus = clip->pathStatus();
    return a;
}

void EditAudioClipPathAction::execute() {
    apply(m_newPath, m_newPathInfo, m_newFormatData, true);
    m_clip->setPathStatus(AudioClip::PathStatus::Normal);
}

void EditAudioClipPathAction::undo() {
    // GUI relinks calculate the hash after committing the path change.
    if (m_newPathInfo.sha512.isEmpty() && m_clip->path() == m_newPath)
        m_newPathInfo.sha512 = m_clip->pathInfo().sha512;
    apply(m_oldPath, m_oldPathInfo, m_oldFormatData, m_hadFormatData);
    m_clip->setPathStatus(m_oldStatus);
}

void EditAudioClipPathAction::apply(const QString &path, const AudioPathInfo &pathInfo,
                                    const QJsonObject &formatData, const bool hasFormatData) const {
    const bool pathChanged = m_clip->path() != path;
    m_clip->setPathInfo(pathInfo);
    if (hasFormatData)
        m_clip->workspace().insert(kFormatDataKey, formatData);
    else
        m_clip->workspace().remove(kFormatDataKey);
    m_clip->setPath(path);
    if (!pathChanged)
        m_clip->notifySourceChanged();
    // The old peak cache is invalid after relinking; clear it after the source notification has
    // synchronously started a replacement decode.
    m_clip->setAudioInfo({});
}
