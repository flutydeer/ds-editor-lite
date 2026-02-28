//
// Created by fluty on 24-9-10.
//

#include "GetPronunciationTask.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"

#include <QDebug>

#include <LangCore/Base/LangCommon.h>
#include <LangCore/Core/Manager.h>

GetPronunciationTask::GetPronunciationTask(const int clipId, const QList<Note *> &notes)
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
    status.title = tr("Fetch Pronunciation");
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

QList<QString> GetPronunciationTask::getPronunciations(const QList<Note *> &notes) const {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }

    const auto singingClip = reinterpret_cast<SingingClip *>(appModel->findClipById(m_clipId));
    const auto singerInfo = singingClip->singerInfo();
    const auto langMgr = LangCore::Manager::instance();

    std::vector<LangCore::G2pInput *> g2pInput;
    for (const auto &note : notes) {
        g2pInput.push_back(
            new LangCore::G2pInput(note->lyric().toStdString(), note->language().toStdString()));
    }

    std::vector<std::string> priorityG2pIds = {};
    if (!singerInfo.isEmpty()) {
        priorityG2pIds.push_back(singerInfo.defaultG2pId().toStdString());
        const auto languages = singerInfo.languages();
        for (const auto &lang : languages)
            priorityG2pIds.push_back(lang.g2p().toStdString());
    }

    const auto g2pResult = langMgr->convert(g2pInput);

    QList<QString> pronResult;
    pronResult.reserve(g2pResult.size());
    for (const auto pNote : g2pResult) {
        Q_ASSERT(!pNote.pronunciation.empty());
        pronResult.append(QString::fromStdString(pNote.pronunciation));
    }
    return pronResult;
}