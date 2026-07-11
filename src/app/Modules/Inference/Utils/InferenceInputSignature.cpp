#include "InferenceInputSignature.h"

#include "Model/AppModel/InferPiece.h"
#include "Model/AppModel/Timeline.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Models/InferParamCurve.h"
#include "Modules/Inference/Models/InferSpeakerMix.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>

namespace {
    QJsonArray doubleArray(const QList<double> &values) {
        QJsonArray array;
        for (const auto value : values)
            array.append(value);
        return array;
    }

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

    QJsonObject commonObject(const InferInputBase &input, const QString &taskType) {
        // This is the semantic task input. Keep it aligned with buildInfer*Input() so the
        // worker payload, cache key, and apply gate all describe the same snapshot.
        return {
            {"taskType",              taskType                          },
            {"clipId",                input.clipId                      },
            {"pieceId",               input.pieceId                     },
            {"headAvailableLengthMs", input.headAvailableLengthMs       },
            {"paddingStartMs",        input.paddingStartMs              },
            {"paddingEndMs",          input.paddingEndMs                },
            {"tempos",                tempoArray(input.timeline)        },
            {"timeSignatures",        timeSignatureArray(input.timeline)},
            {"notes",                 noteArray(input.notes)            },
            {"speaker",               input.speaker                     },
            {"speakerMixSignature",   input.speakerMix.signature()      },
            {"singer",                singerObject(input.identifier)    },
            {"steps",                 input.steps                       },
            {"depth",                 input.depth                       },
            {"pitchSmoothKernelSize", input.pitchSmoothKernelSize       },
        };
    }

    QString hashObject(const QJsonObject &object) {
        const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
        return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex();
    }

    QJsonArray curveArray(const InferParamCurve &curve) {
        return doubleArray(curve.values);
    }
}

namespace InferenceInputSignature {
    QString fromInput(const InferDurationTask::InferDurInput &input) {
        return hashObject(commonObject(input, "duration"));
    }

    QString fromInput(const InferPitchTask::InferPitchInput &input) {
        auto object = commonObject(input, "pitch");
        object.insert("expressiveness", curveArray(input.expressiveness));
        return hashObject(object);
    }

    QString fromInput(const InferVarianceTask::InferVarianceInput &input) {
        auto object = commonObject(input, "variance");
        object.insert("pitch", curveArray(input.pitch));
        return hashObject(object);
    }

    QString fromInput(const InferAcousticTask::InferAcousticInput &input) {
        auto object = commonObject(input, "acoustic");
        object.insert("pitch", curveArray(input.pitch));
        object.insert("breathiness", curveArray(input.breathiness));
        object.insert("tension", curveArray(input.tension));
        object.insert("voicing", curveArray(input.voicing));
        object.insert("energy", curveArray(input.energy));
        object.insert("mouthOpening", curveArray(input.mouthOpening));
        object.insert("gender", curveArray(input.gender));
        object.insert("velocity", curveArray(input.velocity));
        object.insert("toneShift", curveArray(input.toneShift));
        return hashObject(object);
    }

    QString fromCurrentPiece(const QString &taskType, const InferPiece &piece,
                             const SingerIdentifier &identifier) {
        // Rebuild the current piece input on the GUI thread. If this hash matches the task
        // snapshot, a clip-level revision bump came from unrelated edits and is safe to ignore.
        if (taskType == "duration")
            return fromInput(InferControllerHelper::buildInferDurInput(piece, identifier));
        if (taskType == "pitch")
            return fromInput(InferControllerHelper::buildInferPitchInput(piece, identifier));
        if (taskType == "variance")
            return fromInput(InferControllerHelper::buildInferVarianceInput(piece, identifier));
        if (taskType == "acoustic")
            return fromInput(InferControllerHelper::buildInferAcousticInput(piece, identifier));
        return {};
    }
}
