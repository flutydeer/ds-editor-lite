#include "ProjectAutomationDtos.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>

#include <array>

namespace Automation {
    namespace {
        constexpr std::array kParamNames{
            ParamInfo::Pitch,       ParamInfo::Expressiveness, ParamInfo::Energy,
            ParamInfo::Breathiness, ParamInfo::Voicing,        ParamInfo::Tension,
            ParamInfo::MouthOpening, ParamInfo::Gender,         ParamInfo::Velocity,
            ParamInfo::ToneShift,
        };

        CurveDraftDto captureCurveDraft(const Curve &curve) {
            CurveDraftDto result;
            result.localStart = curve.localStart();
            if (curve.type() == Curve::Anchor) {
                result.type = CurveDraftDto::Type::Anchor;
                const auto &anchor = static_cast<const AnchorCurve &>(curve);
                for (const auto *node : anchor.nodes()) {
                    result.nodes.append(
                        {node->pos(), node->value(), node->interpMode()});
                }
                return result;
            }

            const auto &draw = static_cast<const DrawCurve &>(curve);
            result.type = CurveDraftDto::Type::Draw;
            result.step = draw.step;
            result.values = draw.values();
            return result;
        }

        Curve *createCurve(const CurveDraftDto &draft) {
            if (draft.type == CurveDraftDto::Type::Anchor) {
                auto *curve = new AnchorCurve;
                curve->setLocalStart(draft.localStart);
                for (const auto &nodeDraft : draft.nodes) {
                    auto *node = new AnchorNode(nodeDraft.position, nodeDraft.value);
                    node->setInterpMode(nodeDraft.interpolation);
                    curve->insertNode(node);
                }
                return curve;
            }

            auto *curve = new DrawCurve;
            curve->setLocalStart(draft.localStart);
            curve->step = draft.step;
            curve->setValues(draft.values);
            return curve;
        }

        void applyClipProperties(Clip &clip, const ClipPropertiesDto &properties) {
            clip.setName(properties.name);
            clip.setStart(properties.start);
            clip.setLength(properties.length);
            clip.setClipStart(properties.clipStart);
            clip.setClipLen(properties.clipLen);
            clip.setGain(properties.gain);
            clip.setMute(properties.mute);
        }

        void addString(QCryptographicHash &hash, const QString &value) {
            const auto encoded = value.toUtf8();
            hash.addData(QByteArray::number(encoded.size()));
            hash.addData(":", 1);
            hash.addData(encoded);
        }

        void addInteger(QCryptographicHash &hash, const qint64 value) {
            hash.addData(QByteArray::number(value));
            hash.addData(";", 1);
        }

        void addDouble(QCryptographicHash &hash, const double value) {
            hash.addData(QByteArray::number(value, 'g', 17));
            hash.addData(";", 1);
        }

        void addJsonMap(QCryptographicHash &hash, const QMap<QString, QJsonObject> &values) {
            addInteger(hash, values.size());
            for (auto it = values.cbegin(); it != values.cend(); ++it) {
                addString(hash, it.key());
                const auto encoded = QJsonDocument(it.value()).toJson(QJsonDocument::Compact);
                addInteger(hash, encoded.size());
                hash.addData(encoded);
            }
        }

        void addSpeakerMix(QCryptographicHash &hash,
                           const SpeakerMixModel::SpeakerMixData &input) {
            const auto data = SpeakerMixModel::normalizeSpeakerMixData(input);
            addInteger(hash, static_cast<int>(data.mode));
            addInteger(hash, data.dynamicBypassed);
            addInteger(hash, data.sources.size());
            for (const auto &source : data.sources)
                addString(hash, source.speaker.id());
            addInteger(hash, data.fixedWeights.size());
            for (const auto weight : data.fixedWeights)
                addDouble(hash, weight);
            addInteger(hash, data.dynamicKeyframes.size());
            for (const auto &keyframe : data.dynamicKeyframes) {
                addInteger(hash, keyframe.tick);
                addInteger(hash, keyframe.weights.size());
                for (const auto weight : keyframe.weights)
                    addDouble(hash, weight);
            }
            addString(hash, data.sourcePresetId);
            addString(hash, data.sourcePresetName);
            addInteger(hash, data.sourcePresetDirty);
        }

        void addClipDraft(QCryptographicHash &hash, const ClipDraftDto &draft) {
            addInteger(hash, static_cast<int>(draft.type));
            addString(hash, draft.properties.name);
            addInteger(hash, draft.properties.start);
            addInteger(hash, draft.properties.length);
            addInteger(hash, draft.properties.clipStart);
            addInteger(hash, draft.properties.clipLen);
            addDouble(hash, draft.properties.gain);
            addInteger(hash, draft.properties.mute);
            addString(hash, draft.defaultLanguage);
            addString(hash, draft.audioPath);
            addString(hash, draft.audioPathInfo.relativeDir);
            addString(hash, draft.audioPathInfo.sha512);
            addDouble(hash, draft.properties.trimStartMs);
            addDouble(hash, draft.properties.playLengthMs);
            addDouble(hash, draft.properties.materialLengthMs);
            addInteger(hash, draft.usesTrackVoiceContext);
            addJsonMap(hash, draft.workspace);
            const auto singer = draft.ownSingerInfo.identifier();
            addString(hash, singer.singerId);
            addString(hash, singer.packageId);
            addString(hash, singer.packageVersion.toString());
            addString(hash, draft.ownSpeakerInfo.id());
            addSpeakerMix(hash, draft.ownSpeakerMixData);
            addInteger(hash, draft.audioInfo.sampleRate);
            addInteger(hash, draft.audioInfo.channels);
            addInteger(hash, draft.audioInfo.frames);
            addInteger(hash, draft.audioInfo.peakCache.size());
            for (const auto &[minimum, maximum] : draft.audioInfo.peakCache) {
                addInteger(hash, minimum);
                addInteger(hash, maximum);
            }
            addInteger(hash, draft.notes.size());
            for (const auto &note : draft.notes) {
                addInteger(hash, note.localStart);
                addInteger(hash, note.length);
                addInteger(hash, note.keyIndex);
                addInteger(hash, note.centShift);
                addString(hash, note.lyric);
                addString(hash, note.language);
                addString(hash, note.pronunciation.original);
                addString(hash, note.pronunciation.edited);
                for (const auto &candidate : note.pronunciationCandidates)
                    addString(hash, candidate);
                hash.addData(QJsonDocument(note.phonemes.serialize()).toJson(QJsonDocument::Compact));
                addInteger(hash, note.lineFeed);
                addJsonMap(hash, note.workspace);
            }
            addInteger(hash, draft.params.size());
            for (const auto &param : draft.params) {
                addInteger(hash, param.name);
                addInteger(hash, param.type);
                addInteger(hash, param.curves.size());
                for (const auto &curve : param.curves) {
                    addInteger(hash, static_cast<int>(curve.type));
                    addInteger(hash, curve.localStart);
                    addInteger(hash, curve.step);
                    for (const auto value : curve.values)
                        addInteger(hash, value);
                    for (const auto &node : curve.nodes) {
                        addInteger(hash, node.position);
                        addInteger(hash, node.value);
                        addInteger(hash, node.interpolation);
                    }
                }
            }
        }
    }

    TrackPropertiesDto trackPropertiesDto(const Track &track) {
        const auto control = track.control();
        return {TrackId(track.id()), track.name(), control.gain(), control.pan(), control.mute(),
                control.solo()};
    }

    ClipPropertiesDto clipPropertiesDto(const Clip &clip) {
        ClipPropertiesDto result;
        result.id = ClipId(clip.id());
        result.name = clip.name();
        result.start = clip.start();
        result.length = clip.length();
        result.clipStart = clip.clipStart();
        result.clipLen = clip.clipLen();
        result.gain = clip.gain();
        result.mute = clip.mute();
        if (clip.clipType() == Clip::Audio) {
            const auto &audio = static_cast<const AudioClip &>(clip);
            if (audio.hasRealTimeAnchor()) {
                result.trimStartMs = audio.trimStartMs();
                result.playLengthMs = audio.playLengthMs();
                result.materialLengthMs = audio.materialLengthMs();
            }
        }
        return result;
    }

    NoteDraftDto noteDraftDto(const Note &note) {
        NoteDraftDto result;
        result.localStart = note.localStart();
        result.length = note.length();
        result.keyIndex = note.keyIndex();
        result.centShift = note.centShift();
        result.lyric = note.lyric();
        result.language = note.language();
        result.pronunciation = note.pronunciation();
        result.pronunciationCandidates = note.pronCandidates();
        result.phonemes = note.phonemes();
        result.lineFeed = note.lineFeed();
        result.workspace = note.workspace();
        return result;
    }

    CurveDraftDto curveDraftDto(const Curve &curve) {
        return captureCurveDraft(curve);
    }

    std::unique_ptr<Note> buildNote(const NoteDraftDto &draft, SingingClip *clip) {
        auto note = std::make_unique<Note>(clip);
        note->setLocalStart(draft.localStart);
        note->setLength(draft.length);
        note->setKeyIndex(draft.keyIndex);
        note->setCentShift(draft.centShift);
        note->setLyric(draft.lyric);
        note->setLanguage(draft.language);
        note->setPronunciation(draft.pronunciation);
        note->setPronCandidates(draft.pronunciationCandidates);
        note->setPhonemes(draft.phonemes);
        note->setLineFeed(draft.lineFeed);
        note->setWorkspace(draft.workspace);
        return note;
    }

    std::unique_ptr<Curve> buildCurve(const CurveDraftDto &draft) {
        return std::unique_ptr<Curve>(createCurve(draft));
    }

    ClipDraftDto clipDraftDto(const Clip &clip) {
        ClipDraftDto result;
        result.properties = clipPropertiesDto(clip);
        result.properties.id = {};
        result.workspace = clip.workspace();
        if (clip.clipType() == Clip::Audio) {
            const auto &audio = static_cast<const AudioClip &>(clip);
            result.type = ClipDraftDto::Type::Audio;
            result.audioPath = audio.path();
            result.audioPathInfo = audio.pathInfo();
            result.audioPathStatus = audio.pathStatus();
            result.audioInfo = audio.audioInfo();
            result.hasRealTimeAnchor = audio.hasRealTimeAnchor();
            return result;
        }

        const auto &singing = static_cast<const SingingClip &>(clip);
        result.type = ClipDraftDto::Type::Singing;
        result.defaultLanguage = singing.defaultLanguage();
        result.usesTrackVoiceContext = singing.usesTrackVoiceContext();
        result.ownSingerInfo = singing.ownSingerInfo();
        result.ownSpeakerInfo = singing.ownSpeakerInfo();
        result.ownSpeakerMixData = singing.ownSpeakerMixData();
        for (const auto *note : singing.notes())
            result.notes.append(noteDraftDto(*note));
        for (const auto name : kParamNames) {
            const auto *param = singing.params.getParamByName(name);
            if (!param)
                continue;
            for (const auto type : {Param::Edited, Param::Envelope}) {
                ParamCurvesDraftDto paramDraft{.name = name, .type = type};
                for (const auto *curve : param->curves(type)) {
                    if (curve && (curve->type() == Curve::Draw || curve->type() == Curve::Anchor))
                        paramDraft.curves.append(captureCurveDraft(*curve));
                }
                if (!paramDraft.curves.isEmpty())
                    result.params.append(std::move(paramDraft));
            }
        }
        return result;
    }

    TrackDraftDto trackDraftDto(const Track &track) {
        const auto control = track.control();
        TrackDraftDto result;
        result.name = track.name();
        result.colorIndex = track.colorIndex();
        result.gain = control.gain();
        result.pan = control.pan();
        result.mute = control.mute();
        result.solo = control.solo();
        result.defaultLanguage = track.defaultLanguage();
        result.singerInfo = track.singerInfo();
        result.speakerInfo = track.speakerInfo();
        result.speakerMixData = track.speakerMixData();
        for (const auto *clip : track.clips())
            result.clips.append(clipDraftDto(*clip));
        return result;
    }

    std::unique_ptr<Clip> buildClip(const ClipDraftDto &draft, const Track *targetTrack,
                                    const Timeline &timeline) {
        std::unique_ptr<Clip> result;
        if (draft.type == ClipDraftDto::Type::Audio) {
            auto audio = std::make_unique<AudioClip>();
            audio->setPath(draft.audioPath);
            audio->setPathInfo(draft.audioPathInfo);
            audio->setPathStatus(draft.audioPathStatus);
            audio->setAudioInfo(draft.audioInfo);
            if (draft.hasRealTimeAnchor) {
                audio->setRealTimeAnchor(draft.properties.trimStartMs,
                                         draft.properties.playLengthMs,
                                         draft.properties.materialLengthMs);
            }
            result = std::move(audio);
        } else {
            auto singing = std::make_unique<SingingClip>();
            singing->setDefaultLanguage(draft.defaultLanguage);
            for (const auto &noteDraft : draft.notes) {
                auto note = buildNote(noteDraft, singing.get());
                singing->insertNote(note.release());
            }
            if (targetTrack) {
                singing->setTrackVoiceContext(targetTrack->singerInfo(), targetTrack->speakerInfo(),
                                              targetTrack->speakerMixData());
            }
            if (!draft.usesTrackVoiceContext) {
                singing->setOwnVoiceContext(draft.ownSingerInfo, draft.ownSpeakerInfo,
                                            draft.ownSpeakerMixData);
            }
            for (const auto &paramDraft : draft.params) {
                auto *param = singing->params.getParamByName(paramDraft.name);
                if (!param)
                    continue;
                QList<Curve *> curves;
                curves.reserve(paramDraft.curves.size());
                for (const auto &curve : paramDraft.curves)
                    curves.append(createCurve(curve));
                param->setCurves(paramDraft.type, curves, singing.get());
            }
            result = std::move(singing);
        }

        applyClipProperties(*result, draft.properties);
        result->workspace() = draft.workspace;
        if (result->clipType() == Clip::Audio) {
            auto *audio = static_cast<AudioClip *>(result.get());
            if (audio->hasRealTimeAnchor())
                audio->updateTicksFromTruth(timeline);
            else
                audio->syncTruthFromTicks(timeline);
        }
        return result;
    }

    std::unique_ptr<Track> buildTrack(const TrackDraftDto &draft, const Timeline &timeline) {
        auto result = std::make_unique<Track>();
        result->setName(draft.name);
        result->setColorIndex(draft.colorIndex);
        TrackControl control;
        control.setGain(draft.gain);
        control.setPan(draft.pan);
        control.setMute(draft.mute);
        control.setSolo(draft.solo);
        result->setControl(control);
        result->setDefaultLanguage(draft.defaultLanguage);
        result->setVoiceContext(draft.singerInfo, draft.speakerInfo, draft.speakerMixData);
        for (const auto &clipDraft : draft.clips) {
            auto clip = buildClip(clipDraft, result.get(), timeline);
            result->insertClip(clip.release());
        }
        return result;
    }

    QByteArray fingerprint(const TrackPropertiesDto &properties) {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream << properties.id.value() << properties.name << properties.gain << properties.pan
               << properties.mute << properties.solo;
        return result;
    }

    QByteArray fingerprint(const ClipPropertiesDto &properties) {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream << properties.id.value() << properties.name << properties.start << properties.length
               << properties.clipStart << properties.clipLen << properties.gain << properties.mute
               << properties.trimStartMs << properties.playLengthMs << properties.materialLengthMs;
        return result;
    }

    QByteArray fingerprint(const TrackDraftDto &draft) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addString(hash, draft.name);
        addInteger(hash, draft.colorIndex);
        addDouble(hash, draft.gain);
        addDouble(hash, draft.pan);
        addInteger(hash, draft.mute);
        addInteger(hash, draft.solo);
        addString(hash, draft.defaultLanguage);
        const auto singer = draft.singerInfo.identifier();
        addString(hash, singer.singerId);
        addString(hash, singer.packageId);
        addString(hash, singer.packageVersion.toString());
        addString(hash, draft.speakerInfo.id());
        addSpeakerMix(hash, draft.speakerMixData);
        addInteger(hash, draft.clips.size());
        for (const auto &clip : draft.clips)
            addClipDraft(hash, clip);
        return hash.result();
    }

    QByteArray fingerprint(const QList<ClipInsertDto> &clips) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        for (const auto &item : clips) {
            addInteger(hash, item.trackId.value());
            addClipDraft(hash, item.clip);
        }
        return hash.result();
    }

} // namespace Automation
