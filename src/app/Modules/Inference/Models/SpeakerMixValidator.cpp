#include "SpeakerMixValidator.h"

#include <QDebug>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
    /// Compute the average proportion of a source (representative weight for
    /// fallback selection / renormalization). Returns 0 for empty proportions.
    double averageProportion(const InferSpeakerMixSource &source) {
        if (source.proportions.isEmpty())
            return 0.0;
        double sum = 0.0;
        for (const auto p : source.proportions)
            sum += p;
        return sum / source.proportions.size();
    }

    /// Renormalize the sources' proportions in-place so that per-frame sums
    /// equal 1.0 (or close to it). Empty/zero-sum frames are left as zero.
    /// This preserves the relative shape of each source's curve.
    void renormalizeSources(QList<InferSpeakerMixSource> &sources) {
        if (sources.isEmpty())
            return;
        const int frameCount = sources.front().proportions.size();
        for (int frame = 0; frame < frameCount; ++frame) {
            double frameSum = 0.0;
            for (const auto &s : sources) {
                if (frame < s.proportions.size())
                    frameSum += s.proportions[frame];
            }
            if (frameSum <= 0.0 || !std::isfinite(frameSum))
                continue;
            for (auto &s : sources) {
                if (frame < s.proportions.size())
                    s.proportions[frame] = s.proportions[frame] / frameSum;
            }
        }
    }

    /// Pick the fallback speaker from the surviving sources (max average weight).
    QString pickFallbackFromSources(const QList<InferSpeakerMixSource> &sources,
                                    const QString &preferred) {
        if (!preferred.isEmpty()) {
            for (const auto &s : sources) {
                if (s.speaker == preferred)
                    return preferred;
            }
        }
        const InferSpeakerMixSource *best = nullptr;
        double bestWeight = -1.0;
        for (const auto &s : sources) {
            if (s.speaker.isEmpty())
                continue;
            const auto w = averageProportion(s);
            if (w > bestWeight) {
                bestWeight = w;
                best = &s;
            }
        }
        return best ? best->speaker : QString();
    }

    QString formatWarning(const SingerInfo &singer, const QStringList &dropped,
                          const QString &primarySpeaker) {
        return QStringLiteral("Singer %1 (pkg %2@%3): dropped speakers not in mixableSpeakers: %4; "
                              "fallback to [%5]")
            .arg(singer.singerId())
            .arg(singer.packageId())
            .arg(singer.packageVersion().toString())
            .arg(dropped.join(", "))
            .arg(primarySpeaker);
    }
}

SpeakerMixValidator::Result
    SpeakerMixValidator::validate(const QString &speaker,
                                  const InferSpeakerMix &mix,
                                  const SingerInfo &singer) {
    Result result;
    result.sanitizedMix = mix;
    result.primarySpeaker = speaker;

    // 1. Conservative pass-through when singer metadata is not fully resolved.
    //    Avoids blocking inference on Pending/Missing states; the host UI is
    //    responsible for surfacing resolution errors separately.
    if (singer.resolutionState() != ResolutionState::Resolved) {
        if (result.primarySpeaker.isEmpty() && !mix.sources.isEmpty())
            result.primarySpeaker = mix.sources.first().speaker;
        if (result.primarySpeaker.isEmpty() && !mix.fallbackSpeaker.isEmpty())
            result.primarySpeaker = mix.fallbackSpeaker;
        return result;
    }

    // 2. Collect available speaker name set.
    QSet<QString> available;
    bool capabilityAvailable = false;
    if (singer.capability()) {
        const auto &cap = *singer.capability();
        if (!cap.mixableSpeakers.isEmpty()) {
            for (const auto &spk : cap.mixableSpeakers)
                available.insert(spk);
            capabilityAvailable = true;
        }
    }
    if (!capabilityAvailable) {
        // capability nullopt or Inconsistent (mixableSpeakers empty):
        // degrade to the singer's full speaker list so we still catch
        // references to spks that don't exist at all in the voicebank.
        for (const auto &spk : singer.speakers())
            available.insert(spk.id());
    }

    // If we have no available speaker set to validate against (both capability
    // and speakers empty — legacy lite data), pass through conservatively to
    // avoid blocking inference on stale metadata. This mirrors the
    // Pending/Missing pass-through in step 1.
    if (available.isEmpty()) {
        if (result.primarySpeaker.isEmpty() && !mix.sources.isEmpty())
            result.primarySpeaker = mix.sources.first().speaker;
        if (result.primarySpeaker.isEmpty() && !mix.fallbackSpeaker.isEmpty())
            result.primarySpeaker = mix.fallbackSpeaker;
        return result;
    }

    // 3. Resolve primary speaker.
    if (!result.primarySpeaker.isEmpty() && !available.contains(result.primarySpeaker)) {
        result.droppedSpeakers.append(result.primarySpeaker);
        result.primarySpeaker.clear();
    }
    if (result.primarySpeaker.isEmpty()) {
        // Try mix.fallbackSpeaker next.
        if (!mix.fallbackSpeaker.isEmpty() && available.contains(mix.fallbackSpeaker)) {
            result.primarySpeaker = mix.fallbackSpeaker;
        } else {
            if (!mix.fallbackSpeaker.isEmpty() && !available.contains(mix.fallbackSpeaker))
                result.droppedSpeakers.append(mix.fallbackSpeaker);
            // Pick from mix.sources (available ones).
            for (const auto &s : mix.sources) {
                if (!s.speaker.isEmpty() && available.contains(s.speaker)) {
                    result.primarySpeaker = s.speaker;
                    break;
                }
            }
            // Last resort: first speaker in singer.speakers().
            if (result.primarySpeaker.isEmpty() && !singer.speakers().isEmpty())
                result.primarySpeaker = singer.speakers().first().id();
        }
    }

    // 4. Validate mix.sources.
    if (mix.isEmpty()) {
        // Single-speaker mode: synthesize a static mix from primarySpeaker.
        result.sanitizedMix = InferSpeakerMixModel::staticSpeakerMix(result.primarySpeaker);
        // status stays Ok unless primarySpeaker itself was dropped.
        if (!result.droppedSpeakers.isEmpty())
            result.status = Status::Invalid;
    } else {
        QList<InferSpeakerMixSource> validSources;
        validSources.reserve(mix.sources.size());
        for (const auto &s : mix.sources) {
            if (s.speaker.isEmpty()) {
                continue;
            }
            if (!available.contains(s.speaker)) {
                if (!result.droppedSpeakers.contains(s.speaker))
                    result.droppedSpeakers.append(s.speaker);
                continue;
            }
            validSources.append(s);
        }

        if (validSources.isEmpty()) {
            // All sources dropped: fall back to static mix of primarySpeaker.
            result.sanitizedMix = InferSpeakerMixModel::staticSpeakerMix(result.primarySpeaker);
            result.status = Status::Invalid;
        } else if (validSources.size() == mix.sources.size()) {
            // All sources valid: keep mix unchanged (but maybe adjust fallback
            // if the original fallbackSpeaker was dropped).
            result.sanitizedMix = mix;
            if (!mix.fallbackSpeaker.isEmpty() && !available.contains(mix.fallbackSpeaker)) {
                if (!result.droppedSpeakers.contains(mix.fallbackSpeaker))
                    result.droppedSpeakers.append(mix.fallbackSpeaker);
                result.sanitizedMix.fallbackSpeaker =
                    pickFallbackFromSources(validSources, result.primarySpeaker);
            }
            if (result.droppedSpeakers.isEmpty())
                result.status = Status::Ok;
            else
                result.status = Status::Degraded; // primary or fallback dropped, sources intact
        } else {
            // Partial drop: keep validSources, renormalize proportions, pick
            // a new fallbackSpeaker if needed.
            renormalizeSources(validSources);
            InferSpeakerMix sanitized;
            sanitized.fallbackSpeaker = pickFallbackFromSources(validSources, result.primarySpeaker);
            sanitized.sources = std::move(validSources);
            result.sanitizedMix = std::move(sanitized);
            result.status = Status::Degraded;
        }
    }

    // 5. Build warning message (only when something was dropped).
    if (!result.droppedSpeakers.isEmpty()) {
        result.warningMessage = formatWarning(singer, result.droppedSpeakers, result.primarySpeaker);
    }

    return result;
}
