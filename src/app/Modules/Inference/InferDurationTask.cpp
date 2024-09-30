//
// Created by OrangeCat on 24-9-4.
//

#include "InferDurationTask.h"

#include "InferEngine.h"
#include "Model/Inference/GenericInferModel.h"
#include "Utils/JsonUtils.h"

#include <QThread>
#include <QDebug>
#include <QJsonDocument>
#include <utility>

namespace dsonnxinfer {
    struct Segment;
}

bool InferDurationTask::InferDurInput::operator==(const InferDurInput &other) const {
    return clipId == other.clipId && pieceId == other.pieceId && notes == other.notes &&
           configPath == other.configPath && qFuzzyCompare(tempo, other.tempo);
}

int InferDurationTask::clipId() const {
    return m_input.clipId;
}

int InferDurationTask::pieceId() const {
    return m_input.pieceId;
}

InferDurationTask::InferDurationTask(InferDurInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = "推理音素长度";
    status.message = "正在等待：" + m_previewText;
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferDurationTask::InferDurInput InferDurationTask::input(){
    QMutexLocker locker(&m_mutex);
    return m_input;
}

QList<InferDurNote> InferDurationTask::result() {
    QMutexLocker locker(&m_mutex);
    return m_result.notes;
}

void InferDurationTask::runTask() {
    qDebug() << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = "正在推理: " + m_previewText;
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    if (!inferEngine->runLoadConfig(m_input.configPath)) {
        qCritical() << "Task failed";
        return;
    }
    if (isTerminateRequested()) {
        abort();
        return;
    }

    QString resultJson;
    QString errorMessage;
    if (!inferEngine->inferDuration(buildInputJson(), resultJson, errorMessage)) {
        qCritical() << "Task failed";
    }
    if (isTerminateRequested()) {
        abort();
        return;
    }

    success = processOutput(resultJson);
    qInfo() << "时长推理任务正常完成 clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferDurationTask::abort() {
    auto newStatus = status();
    newStatus.message = "正在停止: " + m_previewText;
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "时长推理任务被终止 clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferDurationTask::buildPreviewText() {
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.aheadNames)
            m_previewText.append(phoneme + " ");
        for (const auto &phoneme : note.normalNames)
            m_previewText.append(phoneme + " ");
    }
}

QString InferDurationTask::buildInputJson() const {
    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };

    QList<InferWord> words;

    double pos = 0;
    InferWord word;
    auto commitWord = [&] {
        words.append(word);
        word = InferWord();
    };

    double paddingSpLen = 0.1; // s

    auto processFirstNote = [&] {
        auto firstNote = m_input.notes.first();
        InferNote inferNote;
        inferNote.duration = paddingSpLen;
        word.notes.append(inferNote);

        // Add SP
        InferPhoneme inferPhoneme;
        inferPhoneme.token = "SP";
        inferPhoneme.language = "zh";
        word.phones.append(inferPhoneme);

        for (const auto &phoneme : firstNote.aheadNames) {
            inferPhoneme = InferPhoneme();
            inferPhoneme.token = phoneme;
            inferPhoneme.language = "zh";
            word.phones.append(inferPhoneme);
        }
        commitWord();
    };
    processFirstNote();

    int i = 0;
    for (auto &note : m_input.notes) {
        InferNote inferNote;
        inferNote.key = note.key;
        inferNote.duration = pos + tickToSec(note.length);
        word.notes.append(inferNote);

        for (const auto &phoneme : note.normalNames) {
            InferPhoneme inferPhoneme;
            inferPhoneme.token = phoneme;
            inferPhoneme.language = "zh";
            word.phones.append(inferPhoneme);
        }

        if (i < m_input.notes.size() - 1) {
            for (const auto &phoneme : m_input.notes.at(i + 1).aheadNames) {
                InferPhoneme inferPhoneme;
                inferPhoneme.token = phoneme;
                inferPhoneme.language = "zh";
                word.phones.append(inferPhoneme);
            }
        }

        if (!note.isSlur || i == m_input.notes.size() - 1)
            commitWord();
        i++;
    }

    GenericInferModel model;
    model.words = words;
    const QJsonDocument doc{model.serialize()};
    return doc.toJson();
}

bool InferDurationTask::processOutput(const QString &json) {
    QByteArray data = json.toUtf8();
    GenericInferModel model;
    auto object = QJsonDocument::fromJson(data).object();
    // JsonUtils::save(QString("output-%1.json").arg(id()), object);
    auto success = model.deserialize(object);
    if (!success)
        return false;

    QList<std::pair<double, double>> offsets;
    for (const auto &word : model.words) {
        for (const auto &phoneme : word.phones) {
            offsets.append({word.length(), phoneme.start});
        }
    }
    m_result = m_input;
    int i = 1; // Skip SP phoneme
    for (auto &note : m_result.notes) {
        for (int aheadIndex = 0; aheadIndex < note.aheadNames.count(); aheadIndex++) {
            note.aheadOffsets.append(qRound((offsets[i].first - offsets[i].second) * 1000));
            i++;
        }
        for (int normalIndex = 0; normalIndex < note.normalNames.count(); normalIndex++) {
            note.normalOffsets.append(qRound(offsets[i].second * 1000));
            i++;
        }
    }
    return true;
}