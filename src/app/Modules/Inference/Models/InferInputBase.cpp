#include "InferInputBase.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QVersionNumber>

namespace {
    QJsonArray intArray(const QList<int> &values) {
        QJsonArray array;
        for (const auto value : values)
            array.append(value);
        return array;
    }

    QJsonArray phonemeNameArray(const QList<PhonemeName> &values) {
        QJsonArray array;
        for (const auto &value : values)
            array.append(value.serialize());
        return array;
    }

    QJsonArray noteArray(const QList<InferInputNote> &notes) {
        QJsonArray array;
        for (const auto &note : notes) {
            array.append(QJsonObject{
                {"id",             note.id                            },
                {"start",          note.start                         },
                {"length",         note.length                        },
                {"key",            note.key                           },
                {"isRest",         note.isRest                        },
                {"isSlur",         note.isSlur                        },
                {"isPlus",         note.isPlus                        },
                {"languageDictId", note.languageDictId                },
                {"phonemeNames",   phonemeNameArray(note.phonemeNames)},
                {"phonemeOffsets", intArray(note.phonemeOffsets)      },
            });
        }
        return array;
    }

    QJsonArray tempoArray(const Timeline &timeline) {
        QJsonArray array;
        for (const auto &tempo : timeline.tempos)
            array.append(QJsonObject{
                {"pos",   tempo.pos  },
                {"value", tempo.value}
            });
        return array;
    }

    QJsonArray timeSignatureArray(const Timeline &timeline) {
        QJsonArray array;
        for (const auto &signature : timeline.timeSignatures) {
            array.append(QJsonObject{
                {"pos",         signature.pos        },
                {"numerator",   signature.numerator  },
                {"denominator", signature.denominator},
            });
        }
        return array;
    }

    QJsonObject singerObject(const SingerIdentifier &identifier) {
        return {
            {"singerId",       identifier.singerId                 },
            {"packageId",      identifier.packageId                },
            {"packageVersion", identifier.packageVersion.toString()},
        };
    }
}

QJsonArray InferInputBase::doubleArray(const QList<double> &values) {
    QJsonArray array;
    for (const auto value : values)
        array.append(value);
    return array;
}

QJsonObject InferInputBase::semanticObject(const QString &taskType) const {
    // This is the canonical semantic task snapshot. Engine payloads and apply-gate signatures
    // must both be derived from InferInputBase instead of reading live options later.
    return {
        {"taskType",              taskType                    },
        {"clipId",                clipId                      },
        {"pieceId",               pieceId                     },
        {"headAvailableLengthMs", headAvailableLengthMs       },
        {"paddingStartMs",        paddingStartMs              },
        {"paddingEndMs",          paddingEndMs                },
        {"tempos",                tempoArray(timeline)        },
        {"timeSignatures",        timeSignatureArray(timeline)},
        {"notes",                 noteArray(notes)            },
        {"speaker",               speaker                     },
        {"speakerMixSignature",   speakerMix.signature()      },
        {"singer",                singerObject(identifier)    },
        {"steps",                 steps                       },
        {"depth",                 depth                       },
        {"pitchSmoothKernelSize", pitchSmoothKernelSize       },
    };
}

QString InferInputBase::semanticSignature(const QString &taskType, const QJsonObject &extra) const {
    auto object = semanticObject(taskType);
    for (auto it = extra.begin(); it != extra.end(); ++it)
        object.insert(it.key(), it.value());
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex();
}
