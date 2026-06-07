//
// Created by OrangeCat on 24-9-3.
//

#include "GetPhonemeNameTask.h"

#include "Global/AppGlobal.h"
#include "Global/SingingClipSlicerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Language/OnsetMarker/OnsetMarkerMgr.h"
#include "Modules/Language/S2pMgr.h"

#include <QDebug>

GetPhonemeNameTask::GetPhonemeNameTask(const int clipId, const quint64 clipRevision,
                                       const QList<NoteInferenceSnapshot> &notes,
                                       const SingerInfo &singerInfo, const double tempo)
    : m_clipSingerInfo(singerInfo), m_clipId(clipId), m_clipRevision(clipRevision), m_inputs(notes),
      m_tempo(tempo) {
    for (int i = 0; i < notes.count(); i++) {
        const auto &note = notes.at(i);
        m_previewText.append(note.lyric);
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = tr("Fetch Phoneme Name");
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
    qInfo() << "Task created"
            << " clipId:" << m_clipId << "taskId:" << id() << "taskRevision:" << m_clipRevision
            << "noteCount:" << m_inputs.count();
}

int GetPhonemeNameTask::clipId() const {
    return m_clipId;
}

quint64 GetPhonemeNameTask::clipRevision() const {
    return m_clipRevision;
}

QList<int> GetPhonemeNameTask::noteIds() const {
    QList<int> ids;
    ids.reserve(m_inputs.size());
    for (const auto &note : m_inputs)
        ids.append(note.noteId);
    return ids;
}

bool GetPhonemeNameTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

void GetPhonemeNameTask::runTask() {
    qDebug() << "Running task..."
             << "clipId:" << clipId() << "taskId:" << id();
    processNotes();
    qInfo() << "TaskFinished"
            << "clipId:" << clipId() << "taskId:" << id() << "terminate:" << terminated();
}

void GetPhonemeNameTask::processNotes() {
    // qDebug() << "Language module ready, start to process notes";
    auto newStatus = status();
    newStatus.message = "正在处理: " + m_previewText;
    setStatus(newStatus);
    result = getPhonemeNames();
    distributePhonemes();
}

QList<PhonemeNameResult> GetPhonemeNameTask::getPhonemeNames() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }
    const auto s2pMgr = S2pMgr::instance();
    QList<PhonemeNameResult> results;
    results.reserve(m_inputs.size());
    bool allSuccess = true;

    for (const auto &input : m_inputs) {
        PhonemeNameResult result;
        if (input.pronunciation == "SP" || input.pronunciation == "AP") {
            PhonemeName restPhoneme;
            restPhoneme.name = input.pronunciation;
            result.phonemeNames.append(restPhoneme);
            result.success = true;
        } else {
            const auto g2pId = m_clipSingerInfo.g2pId(input.language);
            const auto phonemes = s2pMgr->syllableToPhoneme(m_clipSingerInfo.identifier(), g2pId,
                                                            input.pronunciation);

            if (!phonemes.empty()) {
                const auto onsetMarker = OnsetMarkerMgr::instance()->marker(
                    m_clipSingerInfo.identifier(), input.language);
                if (onsetMarker) {
                    result.phonemeNames = onsetMarker->mark(phonemes, input.language);
                    result.success = !result.phonemeNames.isEmpty();
                }
                if (!result.success) {
                    qCritical() << "Failed to mark onset of phonemes:" << "language:"
                                << input.language << "g2pId:" << g2pId
                                << "pronunciation:" << input.pronunciation
                                << "phonemes:" << phonemes;
                    allSuccess = false;
                }
            } else if (input.pronunciation == "-") {
                result.success = true;
            } else {
                qCritical() << "Failed to get phoneme names of pronunciation: " << "language:"
                            << input.language << "g2pId:" << g2pId
                            << "pronunciation:" << input.pronunciation;
                allSuccess = false;
            }
        }
        results.append(result);
    }

    m_success.store(allSuccess, std::memory_order_release);
    return results;
}

// Syllabification
bool GetPhonemeNameTask::isPlusNote(const QString &lyric) {
    if (lyric.isEmpty())
        return false;
    for (const auto &ch : lyric) {
        if (ch != '+')
            return false;
    }
    return true;
}

std::pair<bool, int> GetPhonemeNameTask::checkTrailingPlus(const QString &lyric) {
    if (!lyric.endsWith('+')) {
        return {false, 0};
    }
    int count = 0;
    for (int i = lyric.length() - 1; i >= 0 && lyric[i] == '+'; --i) {
        ++count;
    }
    return {true, count};
}

// TODO: 应该移动到公共方法？
const QList<GetPhonemeNameTask::Syllable>
    GetPhonemeNameTask::splitSyllables(const QList<PhonemeName> &phonemes) {
    QList<Syllable> syllables;
    Syllable currentSyllable;
    bool hasOnset = false;

    for (const auto &phoneme : phonemes) {
        if (phoneme.isOnset && hasOnset) {
            syllables.append(currentSyllable);
            currentSyllable = Syllable();
            hasOnset = false;
        }
        if (phoneme.isOnset) {
            hasOnset = true;
        }
        currentSyllable.phonemes.append(phoneme);
    }

    if (!currentSyllable.phonemes.isEmpty()) {
        syllables.append(currentSyllable);
    }

    return syllables;
}

void GetPhonemeNameTask::distributePhonemes() {
    const int count = m_inputs.size();
    int i = 0;

    const double gapThresholdMs = 2.0 * SingingClipSlicerGlobal::padBaseLength;
    const int gapThresholdTicks = static_cast<int>(
        std::round(gapThresholdMs * AppGlobal::ticksPerQuarterNote * m_tempo / 60000.0));

    auto tickGap = [this](int a, int b) -> int {
        const auto &noteA = m_inputs[a];
        const auto &noteB = m_inputs[b];
        return noteB.globalStart - (noteA.globalStart + noteA.length);
    };

    while (i < count) {
        const auto &lyric = m_inputs[i].lyric;
        const auto &pron = m_inputs[i].pronunciation;

        if (lyric == "SP" || lyric == "AP" || pron == "-") {
            i++;
            continue;
        }

        // Orphan "+" note: clear any residual phonemes
        if (isPlusNote(lyric)) {
            result[i].phonemeNames.clear();
            i++;
            continue;
        }

        // Find trailing "+" and "-" notes
        QList<int> plusIndices;
        int j = i + 1;
        int prevIdx = i;
        while (j < count) {
            const auto &nextLyric = m_inputs[j].lyric;
            const auto &nextPron = m_inputs[j].pronunciation;

            if (tickGap(prevIdx, j) > gapThresholdTicks)
                break;

            if (isPlusNote(nextLyric)) {
                plusIndices.append(j);
                prevIdx = j;
                j++;
            } else if (nextPron == "-") {
                prevIdx = j;
                j++;
            } else {
                break;
            }
        }

        auto [hasWordPlus, wordExtraCount] = checkTrailingPlus(lyric);
        if (plusIndices.isEmpty() && !hasWordPlus) {
            i = j;
            continue;
        }

        auto syllables = splitSyllables(result[i].phonemeNames);
        if (syllables.isEmpty()) {
            i = j;
            continue;
        }

        int groupIdx = 0;

        // Word note: default 1 group, trailing '+' adds more
        int wordGroupCount = 1 + (hasWordPlus ? wordExtraCount : 0);
        {
            QList<PhonemeName> merged;
            for (int c = 0; c < wordGroupCount && groupIdx < syllables.size(); c++, groupIdx++)
                merged.append(syllables[groupIdx].phonemes);
            result[i].phonemeNames = merged;
        }

        // Distribute to "+" notes
        for (int pi = 0; pi < plusIndices.size(); pi++) {
            int idx = plusIndices[pi];
            int plusCount = m_inputs[idx].lyric.length();
            bool isLast = (pi == plusIndices.size() - 1);

            if (isLast) {
                // Last "+" greedily takes all remaining groups
                QList<PhonemeName> merged;
                for (int g = groupIdx; g < syllables.size(); g++)
                    merged.append(syllables[g].phonemes);
                result[idx].phonemeNames = merged;
                result[idx].success = !merged.isEmpty();
                groupIdx = syllables.size();
            } else {
                // Non-last "+" takes `plusCount` groups
                QList<PhonemeName> merged;
                for (int c = 0; c < plusCount && groupIdx < syllables.size(); c++, groupIdx++)
                    merged.append(syllables[groupIdx].phonemes);
                result[idx].phonemeNames = merged;
                result[idx].success = !merged.isEmpty();
            }
        }

        i = j;
    }
}
