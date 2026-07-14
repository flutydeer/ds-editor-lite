//
// Created by fluty on 24-9-10.
//

#include "GetPronunciationTask.h"

#include "Model/AppStatus/AppStatus.h"

#include <QDebug>
#include <QLoggingCategory>

#include <map>
#include <optional>

#include <synthrt/G2P/Base/LangCommon.h>

#include "Modules/Language/G2pConvertRunner.h"
#include "Modules/Language/G2pInputAdapter.h"
#include "Modules/SynthrtEngine/SynthrtEngine.h"

Q_LOGGING_CATEGORY(logInferPron, "infer.pronunciation")

namespace {
    std::string toUtf8(const QString &value) {
        const auto bytes = value.toUtf8();
        return {bytes.constData(), static_cast<size_t>(bytes.size())};
    }

    QString fromUtf8(const std::string &value) {
        return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    }

    /// 将 G2pErrorType 转为可读字符串，方便日志排查
    QString g2pErrorTypeName(srt::g2p::G2pErrorType type) {
        switch (type) {
            case srt::g2p::NoError:
                return QStringLiteral("NoError");
            case srt::g2p::InvalidLyric:
                return QStringLiteral("InvalidLyric");
            case srt::g2p::ModelInferenceFailed:
                return QStringLiteral("ModelInferenceFailed");
            case srt::g2p::PhonemeGenerationFailed:
                return QStringLiteral("PhonemeGenerationFailed");
            case srt::g2p::DriverUnavailable:
                return QStringLiteral("DriverUnavailable");
            case srt::g2p::NotInitialized:
                return QStringLiteral("NotInitialized");
            case srt::g2p::UnknownError:
                return QStringLiteral("UnknownError");
            default:
                return QStringLiteral("Unknown(%1)").arg(type);
        }
    }
}

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

QStringList
    GetPronunciationTask::getPronunciations(const QList<NoteInferenceSnapshot> &notes) const {
    // 预填 copy fallback：语言模块未就绪或后续 G2P 失败时均使用原 lyric
    QStringList pronResult;
    pronResult.resize(notes.count());
    for (int i = 0; i < notes.count(); i++)
        pronResult[i] = notes.at(i).lyric;

    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qCCritical(logInferPron) << "Language module not ready yet, using copy fallback";
        return pronResult;
    }

    // R9: resolutionState 预检查（lite UI 状态关注点）。
    // 非 Resolved 视为不可路由，直接 copy fallback（与 GetPhonemeNameTask 语义对齐）。
    if (m_singerInfo.resolutionState() != ResolutionState::Resolved) {
        qCWarning(logInferPron) << "SingerInfo not resolved, skip pronunciation fetch. identifier:"
                                << m_singerInfo.identifier();
        return pronResult;
    }

    auto isSkippedNote = [](const NoteInferenceSnapshot &note) {
        const auto lyric = note.lyric.trimmed();
        if (lyric == "SP" || lyric == "AP" || lyric == "-")
            return true;
        return lyric.count('+') == lyric.length();
    };

    // SynthrtEngine 统一解析并缓存路由。推理链路不回退官方，路由失败时直接 copy fallback。
    std::vector<srt::g2p::G2pInput> requests;
    QList<int> convertIndices; // 仅记录参与 convert 的音符在 notes 中的下标
    std::map<QString, std::optional<srt::g2p::LanguageRoute>> routeCache;

    for (int i = 0; i < notes.count(); i++) {
        const auto &note = notes.at(i);
        if (isSkippedNote(note)) {
            pronResult[i] = note.lyric.trimmed();
            continue;
        }

        auto lyric = note.lyric;
        while (lyric.endsWith('+'))
            lyric.chop(1);

        auto routeIt = routeCache.find(note.language);
        if (routeIt == routeCache.end()) {
            const auto route = SynthrtEngine::instance().resolveLanguageRoute(
                m_singerInfo.identifier(), note.language);
            if (!route.hasValue()) {
                qCWarning(logInferPron).nospace()
                    << "G2P route invalid for lang='" << note.language
                    << "': " << fromUtf8(route.error().message()) << ". Using copy fallback.";
                routeIt = routeCache.emplace(note.language, std::nullopt).first;
            } else {
                routeIt = routeCache.emplace(note.language, *route).first;
            }
        }
        if (!routeIt->second) {
            // 推理链路绝不静默回退官方
            // 用户后续可在 PronunciationView 手动调整
            pronResult[i] = lyric;
            continue;
        }

        requests.push_back(G2pInputAdapter::fromRoute(toUtf8(lyric), *routeIt->second));
        convertIndices.append(i);
    }

    if (requests.empty())
        return pronResult;

    // 统一 Never 策略，声库 G2P 失败即 copy fallback（LangCore 在 pronunciation 中回填 lyric）
    const auto outcomes =
        G2pConvertRunner::convert(SynthrtEngine::instance().languageService(), requests);

    // 调用方负责结果数量与 copy fallback，不再用 Q_ASSERT（Debug 构建下 abort 与 D11
    // 精确报错相悖）。
    if (outcomes.size() != requests.size()) {
        qCWarning(logInferPron).nospace()
            << "G2pConvertRunner returned " << outcomes.size() << " outcomes for "
            << requests.size() << " requests; using copy fallback for all convert notes";
        for (int j = 0; j < convertIndices.size(); ++j)
            pronResult[convertIndices.at(j)] = fromUtf8(requests.at(j).lyric);
        return pronResult;
    }

    for (int i = 0; i < outcomes.size(); i++) {
        pronResult[convertIndices.at(i)] = fromUtf8(outcomes.at(i).pronunciation);

        // 失败诊断（不阻塞）：copy fallback 时 LangCore 已设 pronunciation=lyric
        // 注：使用 qPrintable() 避免 Qt6 QDebug 对 QString 自动加引号
        // （否则空字符串显示为 ""，导致 context='""' 误导排查方向）
        if (outcomes.at(i).errorType != srt::g2p::NoError) {
            qCWarning(logInferPron).nospace()
                << "G2P conversion error note[" << convertIndices.at(i) << "] g2pId='"
                << qPrintable(fromUtf8(outcomes.at(i).g2pId)) << "' context='"
                << qPrintable(fromUtf8(outcomes.at(i).g2pContext))
                << "' source=" << qPrintable(fromUtf8(outcomes.at(i).g2pSource))
                << " errorType=" << outcomes.at(i).errorType << " ("
                << qPrintable(g2pErrorTypeName(outcomes.at(i).errorType)) << ") lyric='"
                << qPrintable(fromUtf8(requests.at(i).lyric)) << "' pronunciation='"
                << qPrintable(fromUtf8(outcomes.at(i).pronunciation)) << "'";
        }
    }

    return pronResult;
}
