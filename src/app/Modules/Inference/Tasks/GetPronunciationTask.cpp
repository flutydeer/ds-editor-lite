//
// Created by fluty on 24-9-10.
//

#include "GetPronunciationTask.h"

#include "Model/AppModel/Note.h"
#include "Model/AppStatus/AppStatus.h"

#include <QDebug>
#include <language-manager/ILanguageManager.h>

GetPronunciationTask::GetPronunciationTask(int clipId, const QList<Note *> &notes)
    : m_clipId(clipId), m_notes(notes) {
    notesRef = notes;
    for (int i = 0; i < notes.count(); i++) {
        m_previewText.append(notes.at(i)->lyric());
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = "获取发音";
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
    qInfo() << "创建获取发音任务"
            << "clipId:" << clipId << "taskId:" << id();
}

int GetPronunciationTask::clipId() const {
    return m_clipId;
}

void GetPronunciationTask::runTask() {
    qDebug() << "运行获取发音任务"
             << "clipId:" << clipId() << "taskId:" << id();
    result = getPronunciations(m_notes);
    qInfo() << "获取发音任务完成 taskId:" << id() << "terminate:" << terminated();
}

QList<QString> GetPronunciationTask::getPronunciations(const QList<Note *> &notes){
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }
    const auto langMgr = LangMgr::ILanguageManager::instance();
    QList<LangNote *> langNotes;
    for (const auto note : notes) {
        const auto language = note->language() == "unknown" ? "unknown" : note->language();
        const auto category =
            note->language() == "unknown" ? "unknown" : langMgr->language(language)->category();
        langNotes.append(new LangNote(note->lyric(), language, category));
    }

    langMgr->correct(langNotes);
    langMgr->convert(langNotes);

    QList<QString> result;
    for (const auto pNote : langNotes) {
        Q_ASSERT(!pNote->syllable.isEmpty());
        result.append(pNote->syllable);
        delete pNote;
    }
    return result;
}