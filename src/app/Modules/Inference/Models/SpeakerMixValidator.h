#ifndef SPEAKERMIXVALIDATOR_H
#define SPEAKERMIXVALIDATOR_H

#include "Model/AppModel/InferSpeakerMix.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

#include <QString>
#include <QStringList>

/// SpeakerMixValidator - Pre-inference speaker validation for a piece.
///
/// Ensures the speaker + speakerMix passed to inference reference only
/// speakers that actually exist (and are mixable) in the resolved singer's
/// capabilityReport. Handles legacy editor configs where a project file
/// references spks that have since been removed/renamed in a same-name
/// same-version voicebank update.
///
/// Behavior:
///  - singer not Resolved (Pending/Missing): conservative pass-through
///    (return Ok + original mix) to avoid blocking inference on transient
///    states; host UI is responsible for surfacing resolution errors.
///  - singer.capability() == nullopt (pure G2P / legacy lite data):
///    degrade to singer.speakers() id set as available.
///  - mixableSpeakers empty (Inconsistent): same degradation as nullopt.
///  - sources partially invalid: drop invalid sources, renormalize
///    remaining weights, return Degraded.
///  - all sources invalid: fall back to staticSpeakerMix(primarySpeaker),
///    return Invalid.
class SpeakerMixValidator {
public:
    enum class Status {
        Ok,        ///< all speakers valid, mix unchanged
        Degraded,  ///< some speakers dropped, mix renormalized
        Invalid,   ///< all speakers dropped, mix fell back to static
    };

    struct Result {
        Status status = Status::Ok;
        InferSpeakerMix sanitizedMix;     ///< filtered mix safe for inference
        QString primarySpeaker;           ///< authoritative speaker name for input.speaker
        QStringList droppedSpeakers;      ///< invalid speaker names (for UI / log)
        QString warningMessage;           ///< single-line summary for qWarning

        bool ok() const { return status == Status::Ok; }
    };

    /// Validate (speaker, mix) against singer's mixableSpeakers.
    /// Does NOT throw; never returns a mix that references unavailable speakers.
    static Result validate(const QString &speaker,
                           const InferSpeakerMix &mix,
                           const SingerInfo &singer);

private:
    SpeakerMixValidator() = delete;
};

#endif // SPEAKERMIXVALIDATOR_H
