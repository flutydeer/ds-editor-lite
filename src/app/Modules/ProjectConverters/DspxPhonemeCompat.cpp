#include "DspxPhonemeCompat.h"

#include <QJsonArray>

#include <optional>

namespace {
    constexpr const char *kSnapshotKey = "dspxSnapshot";

    opendspx::Phoneme encodePhoneme(const PhonemeName &name, const int start) {
        opendspx::Phoneme result;
        result.language = name.language.toStdString();
        result.token = name.name.toStdString();
        result.start = start;
        result.onset = name.isOnset;
        return result;
    }

    std::vector<opendspx::Phoneme> encodeSequence(const QList<PhonemeName> &names,
                                                  const QList<int> &offsets) {
        std::vector<opendspx::Phoneme> result;
        if (names.count() != offsets.count())
            return result;

        result.reserve(names.count());
        for (qsizetype i = 0; i < names.count(); ++i)
            result.push_back(encodePhoneme(names.at(i), offsets.at(i)));
        return result;
    }

    bool phonemeEquals(const opendspx::Phoneme &lhs, const opendspx::Phoneme &rhs) {
        return lhs.language == rhs.language && lhs.token == rhs.token && lhs.start == rhs.start &&
               lhs.onset == rhs.onset;
    }

    bool sequenceEquals(const std::vector<opendspx::Phoneme> &lhs,
                        const std::vector<opendspx::Phoneme> &rhs) {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (!phonemeEquals(lhs[i], rhs[i]))
                return false;
        }
        return true;
    }

    bool phonemesEqual(const opendspx::Phonemes &lhs, const opendspx::Phonemes &rhs) {
        return sequenceEquals(lhs.original, rhs.original) &&
               sequenceEquals(lhs.edited, rhs.edited);
    }

    QJsonArray encodeSequenceSnapshot(const std::vector<opendspx::Phoneme> &phonemes) {
        QJsonArray result;
        for (const auto &phoneme : phonemes) {
            result.append(QJsonObject{
                {"language", QString::fromStdString(phoneme.language)},
                {"token",    QString::fromStdString(phoneme.token)   },
                {"start",    phoneme.start                           },
                {"onset",    phoneme.onset                           },
            });
        }
        return result;
    }

    QJsonObject encodeSnapshot(const opendspx::Phonemes &phonemes) {
        return {
            {"original", encodeSequenceSnapshot(phonemes.original)},
            {"edited",   encodeSequenceSnapshot(phonemes.edited)  },
        };
    }

    std::optional<std::vector<opendspx::Phoneme>> decodeSequenceSnapshot(
        const QJsonValue &value) {
        if (!value.isArray())
            return std::nullopt;

        std::vector<opendspx::Phoneme> result;
        const auto array = value.toArray();
        result.reserve(array.size());
        for (const auto &item : array) {
            if (!item.isObject())
                return std::nullopt;
            const auto object = item.toObject();
            if (!object.value("language").isString() || !object.value("token").isString() ||
                !object.value("start").isDouble() || !object.value("onset").isBool()) {
                return std::nullopt;
            }

            opendspx::Phoneme phoneme;
            phoneme.language = object.value("language").toString().toStdString();
            phoneme.token = object.value("token").toString().toStdString();
            phoneme.start = object.value("start").toInt();
            phoneme.onset = object.value("onset").toBool();
            result.push_back(std::move(phoneme));
        }
        return result;
    }

    std::optional<opendspx::Phonemes> decodeSnapshot(const QJsonValue &value) {
        if (!value.isObject())
            return std::nullopt;
        const auto object = value.toObject();
        const auto original = decodeSequenceSnapshot(object.value("original"));
        const auto edited = decodeSequenceSnapshot(object.value("edited"));
        if (!original || !edited)
            return std::nullopt;

        opendspx::Phonemes result;
        result.original = *original;
        result.edited = *edited;
        return result;
    }

    QList<PhonemeName> decodeNames(const std::vector<opendspx::Phoneme> &phonemes) {
        QList<PhonemeName> result;
        result.reserve(static_cast<qsizetype>(phonemes.size()));
        for (const auto &phoneme : phonemes) {
            PhonemeName name;
            name.language = QString::fromStdString(phoneme.language);
            name.name = QString::fromStdString(phoneme.token);
            name.isOnset = phoneme.onset;
            result.append(std::move(name));
        }
        return result;
    }

    QList<int> decodeOffsets(const std::vector<opendspx::Phoneme> &phonemes) {
        QList<int> result;
        result.reserve(static_cast<qsizetype>(phonemes.size()));
        for (const auto &phoneme : phonemes)
            result.append(phoneme.start);
        return result;
    }

    Phonemes decodeStandard(const opendspx::Phonemes &dspxPhonemes) {
        Phonemes result;
        result.nameSeq.original = decodeNames(dspxPhonemes.original);
        result.offsetSeq.original = decodeOffsets(dspxPhonemes.original);

        const auto &effective = dspxPhonemes.edited.empty() ? dspxPhonemes.original
                                                            : dspxPhonemes.edited;
        const auto effectiveNames = decodeNames(effective);
        const auto effectiveOffsets = decodeOffsets(effective);
        if (effectiveNames != result.nameSeq.original)
            result.nameSeq.edited = effectiveNames;
        if (effectiveOffsets != result.offsetSeq.original)
            result.offsetSeq.edited = effectiveOffsets;
        return result;
    }

    bool isStandardEmpty(const opendspx::Phonemes &phonemes) {
        return phonemes.original.empty() && phonemes.edited.empty();
    }
}

namespace DspxPhonemeCompat {

void encode(const Phonemes &phonemes, opendspx::Phonemes &dspxPhonemes,
            QJsonObject &workspacePhoneme) {
    dspxPhonemes.original = encodeSequence(phonemes.nameSeq.original,
                                           phonemes.offsetSeq.original);
    dspxPhonemes.edited.clear();
    if (phonemes.nameSeq.isEdited() || phonemes.offsetSeq.isEdited()) {
        dspxPhonemes.edited = encodeSequence(phonemes.nameSeq.result(),
                                             phonemes.offsetSeq.result());
    }

    const auto serialized = phonemes.serialize();
    workspacePhoneme["name"] = serialized.value("name");
    workspacePhoneme["offset"] = serialized.value("offset");
    workspacePhoneme[kSnapshotKey] = encodeSnapshot(dspxPhonemes);
}

Phonemes decode(const opendspx::Phonemes &dspxPhonemes,
                const QJsonObject *workspacePhoneme) {
    const bool hasWorkspaceData = workspacePhoneme &&
                                  workspacePhoneme->value("name").isObject() &&
                                  workspacePhoneme->value("offset").isObject();
    if (hasWorkspaceData) {
        Phonemes workspaceValue;
        workspaceValue.deserialize(*workspacePhoneme);

        if (workspacePhoneme->contains(kSnapshotKey)) {
            const auto snapshot = decodeSnapshot(workspacePhoneme->value(kSnapshotKey));
            if (snapshot && phonemesEqual(*snapshot, dspxPhonemes))
                return workspaceValue;
        } else if (isStandardEmpty(dspxPhonemes)) {
            return workspaceValue;
        }
    }
    return decodeStandard(dspxPhonemes);
}

}
