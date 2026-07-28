#include "InferInputBase.h"

#include <lite/Support/MathUtils.h>

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

    QJsonArray effectiveTempoArray(const Timeline &timeline, const int startTick,
                                   const int endTick) {
        QJsonArray array;
        array.append(QJsonObject{
            {"pos",   startTick                  },
            {"value", timeline.tempoAt(startTick)}
        });
        for (const auto &tempo : timeline.tempos()) {
            if (tempo.pos <= startTick || tempo.pos >= endTick)
                continue;
            array.append(QJsonObject{
                {"pos",   tempo.pos  },
                {"value", tempo.value}
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

QJsonObject InferInputBase::paramCurveObject(const InferParamCurve &curve) {
    return {
        {"localStartTick", curve.localStartTick     },
        {"values",         doubleArray(curve.values)}
    };
}

QJsonObject InferInputBase::semanticObject(const QString &taskType) const {
    // This is the canonical semantic task snapshot. Engine payloads and apply-gate signatures
    // must both be derived from InferInputBase instead of reading live options later.
    return {
        {"taskType", taskType},
        {"clipId", clipId},
        {"pieceId", pieceId},
        {"clipStartTick", clipStartTick},
        {"pieceStartTick", pieceStartTick},
        {"pieceEndTick", pieceEndTick},
        {"headAvailableLengthMs", headAvailableLengthMs},
        {"paddingStartMs", paddingStartMs},
        {"paddingEndMs", paddingEndMs},
        {"minimumFirstOffsetMs", minimumFirstOffsetMs},
        {"requiredHeadLengthMs", requiredHeadLengthMs},
        {"maximumHeadLengthMs", maximumHeadLengthMs},
        {"tempos", effectiveTempoArray(timeline, pieceStartTick, pieceEndTick)},
        {"notes", noteArray(notes)},
        {"speaker", speaker},
        {"speakerMixSignature", speakerMix.signature()},
        {"singer", singerObject(identifier)},
        {"steps", steps},
        {"depth", depth},
        {"pitchSmoothKernelSize", pitchSmoothKernelSize},
    };
}

QString InferInputBase::semanticSignature(const QString &taskType, const QJsonObject &extra) const {
    auto object = semanticObject(taskType);
    for (auto it = extra.begin(); it != extra.end(); ++it)
        object.insert(it.key(), it.value());
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex();
}

QList<double> InferInputBase::curveSampleSeconds(const InferParamCurve &curve) const {
    QList<double> positions;
    positions.reserve(curve.values.size());
    const double pieceStartSeconds = timeline.tickToSec(pieceStartTick);
    for (qsizetype i = 0; i < curve.values.size(); ++i) {
        const int globalTick = clipStartTick + curve.localStartTick + static_cast<int>(i) * 5;
        positions.append(timeline.tickToSec(globalTick) - pieceStartSeconds);
    }
    return positions;
}

QList<double> InferInputBase::frameSampleSeconds(const int frames,
                                                 const double intervalSeconds) const {
    QList<double> positions;
    if (frames <= 0 || intervalSeconds <= 0)
        return positions;
    positions.reserve(frames);
    for (int i = 0; i < frames; ++i)
        positions.append(i * intervalSeconds);
    return positions;
}

QList<double> InferInputBase::resampleCurveToFrames(const InferParamCurve &curve, const int frames,
                                                    const double intervalSeconds,
                                                    const double emptyValue) const {
    const auto targets = frameSampleSeconds(frames, intervalSeconds);
    if (targets.isEmpty())
        return {};
    if (curve.values.isEmpty()) {
        QList<double> result;
        result.fill(emptyValue, targets.size());
        return result;
    }
    return MathUtils::resample(curve.values, curveSampleSeconds(curve), targets);
}

InferParamCurve InferInputBase::resampleFramesToCurve(const QList<double> &values,
                                                      const double intervalSeconds) const {
    InferParamCurve result;
    if (values.isEmpty() || intervalSeconds <= 0 || pieceEndTick <= pieceStartTick)
        return result;

    const int localPieceStart = pieceStartTick - clipStartTick;
    const int localPieceEnd = pieceEndTick - clipStartTick;
    result.localStartTick = qRound(static_cast<double>(localPieceStart) / 5.0) * 5;
    const int targetCount =
        qMax(1, qCeil(static_cast<double>(localPieceEnd - result.localStartTick) / 5.0));

    QList<double> sourcePositions;
    sourcePositions.reserve(values.size());
    for (qsizetype i = 0; i < values.size(); ++i)
        sourcePositions.append(i * intervalSeconds);

    QList<double> targetPositions;
    targetPositions.reserve(targetCount);
    const double pieceStartSeconds = timeline.tickToSec(pieceStartTick);
    for (int i = 0; i < targetCount; ++i) {
        const int globalTick = clipStartTick + result.localStartTick + i * 5;
        targetPositions.append(timeline.tickToSec(globalTick) - pieceStartSeconds);
    }
    result.values = MathUtils::resample(values, sourcePositions, targetPositions);
    return result;
}
