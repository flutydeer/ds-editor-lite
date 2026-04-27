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

    const auto singingClip = static_cast<SingingClip *>(appModel->findClipById(m_clipId));
    Q_ASSERT(singingClip);
    const auto singerInfo = singingClip->singerInfo();
    const auto langMgr = LangCore::Manager::instance();

    auto isRestNote = [](const Note *note) {
        const auto lyric = note->lyric().trimmed();
        return lyric == "SP" || lyric == "AP";
    };

    std::vector<LangCore::G2pInput *> g2pInput;
    QList<int> nonRestIndices;
    for (int i = 0; i < notes.count(); i++) {
        const auto &note = notes.at(i);
        if (isRestNote(note)) {
            continue;
        }
        nonRestIndices.append(i);
        const auto g2pId = singerInfo.isEmpty()
                               ? ("g2p-" + note->language() + "-official").toStdString()
                               : singerInfo.g2pId(note->language()).toStdString();
        g2pInput.push_back(
            new LangCore::G2pInput(note->lyric().toStdString(), g2pId));
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
    pronResult.resize(notes.count());

    for (int i = 0; i < notes.count(); i++) {
        if (isRestNote(notes.at(i))) {
            pronResult[i] = notes.at(i)->lyric().trimmed();
        }
    }

    for (int i = 0; i < g2pResult.size(); i++) {
        Q_ASSERT(!g2pResult[i].pronunciation.empty());
        pronResult[nonRestIndices.at(i)] = QString::fromStdString(g2pResult[i].pronunciation);
    }

    return pronResult;
}