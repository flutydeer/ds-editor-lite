//
// Created by fluty on 24-9-16.
//

#ifndef GENERICINFERMODEL_H
#define GENERICINFERMODEL_H

#include "Interface/IHashProvider.h"
#include "Interface/ISerializable.h"

class InferPhoneme final : public ISerializable {
public:
    InferPhoneme() = default;
    InferPhoneme(QString token, const QString &languageDictId, bool is_onset = true, double start = 0);

    QString token;
    QString languageDictId;
    bool is_onset = true;
    double start = 0; // s

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class InferNote final : public ISerializable {
public:
    InferNote() = default;
    InferNote(int key, int cents, double duration, bool is_rest = false, QString glide = "none");

    int key = 0;
    int cents = 0;
    double duration = 0; // s
    bool is_rest = false;
    QString glide = "none";

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class InferWord final : public ISerializable {
public:
    InferWord() = default;
    InferWord(QList<InferPhoneme> phones, QList<InferNote> notes);

    QList<InferPhoneme> phones;
    QList<InferNote> notes;

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    [[nodiscard]] double length() const;
};

class InferRetake final : public ISerializable {
public:
    // index, [start, end)
    int start = 0;
    int end = 0;

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class InferParam final : public ISerializable {
public:
    QString tag;
    bool dynamic = false;
    double interval = 0.01; // s
    QList<double> values;
    InferRetake retake;

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class GenericInferModel final : public ISerializable, public IHashProvider {
public:
    double offset = 0;
    QList<InferWord> words;
    QList<InferParam> params;

    // 元数据，用于区分不同模型
    QString configPath;

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
    [[nodiscard]] QString serializeToJson(bool useMetadata = false) const;
    bool deserializeFromJson(const QString &json);
    [[nodiscard]] QString hashData() const override;
};



#endif // GENERICINFERMODEL_H
