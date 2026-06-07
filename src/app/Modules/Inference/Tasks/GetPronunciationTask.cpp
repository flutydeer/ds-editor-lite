//
// Created by fluty on 24-9-10.
//

#include "GetPronunciationTask.h"

#include "Model/AppStatus/AppStatus.h"

#include <QDebug>

#include <LangCore/Base/LangCommon.h>
#include <LangCore/Core/Manager.h>

GetPronunciationTask::GetPronunciationTask(const int clipId, const quint64 clipRevision,
                                           const QList<NoteInferenceSnapshot> &notes,
                                           const SingerInfo &singerInfo)
    : m_clipId(clipId), m_clipRevision(clipRevision), m_singerInfo(singerInfo), m_notes(notes) {
    for (int i = 0; i < notes.count(); i++) {
        m_previewText.append(notes.at(i).lyric);
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = tr("Fetch Pronunciation");
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
    qInfo() << "创建获取发音任务"
            << "clipId:" << clipId << "taskId:" << id() << "taskRevision:" << m_clipRevision;
}

int GetPronunciationTask::clipId() const {
    return m_clipId;
}

quint64 GetPronunciationTask::clipRevision() const {
    return m_clipRevision;
}

QList<int> GetPronunciationTask::noteIds() const {
    QList<int> ids;
    ids.reserve(m_notes.size());
    for (const auto &note : m_notes)
        ids.append(note.noteId);
    return ids;
}

void GetPronunciationTask::runTask() {
    qDebug() << "运行获取发音任务"
             << "clipId:" << clipId() << "taskId:" << id();
    result = getPronunciations(m_notes);
    qInfo() << "获取发音任务完成 taskId:" << id() << "terminate:" << terminated();
}

QStringList GetPronunciationTask::getPronunciations(
    const QList<NoteInferenceSnapshot> &notes) const {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }

    const auto langMgr = LangCore::Manager::instance();

    auto isSkippedNote = [](const NoteInferenceSnapshot &note) {
        const auto lyric = note.lyric.trimmed();
        if (lyric == "SP" || lyric == "AP" || lyric == "-")
            return true;
        return lyric.count('+') == lyric.length();
    };

    std::vector<LangCore::G2pInput *> g2pInput;
    QList<int> nonRestIndices;
    for (int i = 0; i < notes.count(); i++) {
        const auto &note = notes.at(i);
        if (isSkippedNote(note)) {
            continue;
        }
        nonRestIndices.append(i);
        auto lyric = note.lyric;
        while (lyric.endsWith('+'))
            lyric.chop(1);
        const auto g2pId = ("g2p-" + note.language + "-official").toStdString();
        g2pInput.push_back(new LangCore::G2pInput(lyric.toStdString(), g2pId));
    }

    std::vector<std::string> priorityG2pIds = {};
    if (!m_singerInfo.isEmpty()) {
        priorityG2pIds.push_back(m_singerInfo.defaultG2pId().toStdString());
        const auto languages = m_singerInfo.languages();
        for (const auto &lang : languages)
            priorityG2pIds.push_back(lang.g2p().toStdString());
    }

    const auto g2pResult = langMgr->convert(g2pInput);

    QStringList pronResult;
    pronResult.resize(notes.count());

    for (int i = 0; i < notes.count(); i++) {
        if (isSkippedNote(notes.at(i))) {
            pronResult[i] = notes.at(i).lyric.trimmed();
        }
    }

    for (int i = 0; i < g2pResult.size(); i++) {
        Q_ASSERT(!g2pResult[i].pronunciation.empty());
        pronResult[nonRestIndices.at(i)] = QString::fromStdString(g2pResult[i].pronunciation);
    }

    return pronResult;
}
