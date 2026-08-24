#include "ProjectAutomationDtos.h"

#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/AutomationWire/PublicConstants.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonDocument>
#include <QSet>

#include <array>
#include <cmath>

namespace Automation {
    namespace {
        constexpr std::array kParamNames{
            ParamInfo::Pitch,        ParamInfo::Expressiveness, ParamInfo::Energy,
            ParamInfo::Breathiness,  ParamInfo::Voicing,        ParamInfo::Tension,
            ParamInfo::MouthOpening, ParamInfo::Gender,         ParamInfo::Velocity,
            ParamInfo::ToneShift,
        };

        CurveDraftDto captureCurveDraft(const Curve &curve) {
            CurveDraftDto result;
            result.id = CurveId(curve.id());
            result.localStart = curve.localStart();
            if (curve.type() == Curve::Anchor) {
                result.type = CurveDraftDto::Type::Anchor;
                const auto &anchor = static_cast<const AnchorCurve &>(curve);
                for (const auto *node : anchor.nodes()) {
                    result.nodes.append(
                        {node->pos(), node->value(), node->interpMode(), AnchorId(node->id())});
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
                auto *curve = draft.id.isValid() ? new AnchorCurve(draft.id.value())
                                                 : new AnchorCurve;
                curve->setLocalStart(draft.localStart);
                for (const auto &nodeDraft : draft.nodes) {
                    auto *node = nodeDraft.id.isValid()
                                     ? new AnchorNode(nodeDraft.position, nodeDraft.value,
                                                      nodeDraft.id.value())
                                     : new AnchorNode(nodeDraft.position, nodeDraft.value);
                    node->setInterpMode(nodeDraft.interpolation);
                    curve->insertNode(node);
                }
                return curve;
            }

            auto *curve = draft.id.isValid() ? new DrawCurve(draft.id.value()) : new DrawCurve;
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

        void appendCreatedObject(QList<CreatedObjectRef> *createdObjects, const QString &clientRef,
                                 const ObjectKind kind, const int id) {
            if (createdObjects && !clientRef.isEmpty())
                createdObjects->append({
                    clientRef, {kind, id}
                });
        }

        void collectClientRefs(const NoteDraftDto &draft, QStringList &clientRefs) {
            if (!draft.clientRef.isEmpty())
                clientRefs.append(draft.clientRef);
        }

        void collectClientRefs(const ClipDraftDto &draft, QStringList &clientRefs) {
            if (!draft.clientRef.isEmpty())
                clientRefs.append(draft.clientRef);
            for (const auto &note : draft.notes)
                collectClientRefs(note, clientRefs);
        }

        void collectClientRefs(const TrackDraftDto &draft, QStringList &clientRefs,
                               const bool includeClips = true) {
            if (!draft.clientRef.isEmpty())
                clientRefs.append(draft.clientRef);
            if (!includeClips)
                return;
            for (const auto &clip : draft.clips)
                collectClientRefs(clip, clientRefs);
        }

        AutomationResult<AutomationUnit> validateUniqueClientRefs(const QStringList &clientRefs) {
            QSet<QString> seen;
            for (const auto &clientRef : clientRefs) {
                if (seen.contains(clientRef)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("client_ref"),
                        QStringLiteral("Client references must be unique within a request"));
                }
                seen.insert(clientRef);
            }
            return AutomationUnit{};
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

        void addStringMap(QCryptographicHash &hash, const QMap<QString, QString> &values) {
            addInteger(hash, values.size());
            for (auto it = values.cbegin(); it != values.cend(); ++it) {
                addString(hash, it.key());
                addString(hash, it.value());
            }
        }

        void addStringList(QCryptographicHash &hash, const QStringList &values) {
            addInteger(hash, values.size());
            for (const auto &value : values)
                addString(hash, value);
        }

        void addSpeakerInfo(QCryptographicHash &hash, const SpeakerInfo &speaker) {
            addString(hash, speaker.id());
            addString(hash, speaker.name());
            addStringMap(hash, speaker.localizedNames());
            addString(hash, speaker.toneMin());
            addString(hash, speaker.toneMax());
            const auto toneRange = speaker.toneRange();
            addInteger(hash, toneRange.has_value());
            if (toneRange) {
                addInteger(hash, toneRange->first);
                addInteger(hash, toneRange->second);
            }
            addInteger(hash, speaker.mixable());
        }

        void addLanguageInfo(QCryptographicHash &hash, const LanguageInfo &language) {
            addString(hash, language.id());
            addString(hash, language.name());
            addStringMap(hash, language.localizedNames());
            addString(hash, language.g2p());
            addString(hash, language.dict());
            addString(hash, language.s2pMode());
            addString(hash, language.onsetMode());
            addString(hash, language.s2pFile());
            addString(hash, language.onsetFile());
            addInteger(hash, language.hasG2pPackageVersion());
            addString(hash, language.g2pPackageVersion());
            addStringList(hash, language.g2pPackagePaths());
        }

        void addOptionalStringList(QCryptographicHash &hash,
                                   const std::optional<QStringList> &values) {
            addInteger(hash, values.has_value());
            if (values)
                addStringList(hash, *values);
        }

        void addOptionalBool(QCryptographicHash &hash, const std::optional<bool> value) {
            addInteger(hash, value.has_value());
            if (value)
                addInteger(hash, *value);
        }

        void addSingerCapability(QCryptographicHash &hash,
                                 const std::optional<SingerCapabilitySummary> &capability) {
            addInteger(hash, capability.has_value());
            if (!capability)
                return;
            addStringList(hash, capability->mixableSpeakers);
            addInteger(hash, capability->speakerConsistency);
            addStringList(hash, capability->speakerWarnings);
            addOptionalStringList(hash, capability->acousticParameters);
            addOptionalBool(hash, capability->pitchUsesExpressiveness);
            addOptionalBool(hash, capability->vocoderPitchControllable);
            addStringList(hash, capability->effectivePhonemes);
            addInteger(hash, capability->phonemeConsistency);
            addStringList(hash, capability->phonemeWarnings);
            addInteger(hash, capability->phonemeDegraded);
            addStringList(hash, capability->effectiveLanguages);
            addInteger(hash, capability->languageConsistency);
            addStringList(hash, capability->languageWarnings);
        }

        void addSingerInfo(QCryptographicHash &hash, const SingerInfo &singer) {
            const auto identifier = singer.identifier();
            addString(hash, identifier.singerId);
            addString(hash, identifier.packageId);
            addString(hash, identifier.packageVersion.toString());
            addString(hash, singer.name());
            addStringMap(hash, singer.localizedNames());
            const auto speakers = singer.speakers();
            addInteger(hash, speakers.size());
            for (const auto &speaker : speakers)
                addSpeakerInfo(hash, speaker);
            const auto languages = singer.languages();
            addInteger(hash, languages.size());
            for (const auto &language : languages)
                addLanguageInfo(hash, language);
            addString(hash, singer.defaultLanguage());
            addString(hash, singer.defaultDict());
            addInteger(hash, static_cast<int>(singer.resolutionState()));
            addSingerCapability(hash, singer.capability());
        }

        void addSpeakerMix(QCryptographicHash &hash, const SpeakerMixModel::SpeakerMixData &input) {
            const auto data = SpeakerMixModel::normalizeSpeakerMixData(input);
            addInteger(hash, static_cast<int>(data.mode));
            addInteger(hash, data.dynamicBypassed);
            addInteger(hash, data.sources.size());
            for (const auto &source : data.sources)
                addSpeakerInfo(hash, source.speaker);
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
            addString(hash, draft.clientRef);
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
            addInteger(hash, static_cast<int>(draft.audioPathStatus));
            addInteger(hash, draft.hasRealTimeAnchor);
            addDouble(hash, draft.properties.trimStartMs);
            addDouble(hash, draft.properties.playLengthMs);
            addDouble(hash, draft.properties.materialLengthMs);
            addInteger(hash, draft.usesTrackVoiceContext);
            addJsonMap(hash, draft.workspace);
            addSingerInfo(hash, draft.ownSingerInfo);
            addSpeakerInfo(hash, draft.ownSpeakerInfo);
            addSpeakerMix(hash, draft.ownSpeakerMixData);
            addInteger(hash, draft.audioInfo.chunkSize);
            addInteger(hash, draft.audioInfo.mipmapScale);
            addInteger(hash, draft.audioInfo.sampleRate);
            addInteger(hash, draft.audioInfo.channels);
            addInteger(hash, draft.audioInfo.frames);
            addInteger(hash, draft.audioInfo.peakCache.size());
            for (const auto &[minimum, maximum] : draft.audioInfo.peakCache) {
                addInteger(hash, minimum);
                addInteger(hash, maximum);
            }
            addInteger(hash, draft.audioInfo.peakCacheMipmap.size());
            for (const auto &[minimum, maximum] : draft.audioInfo.peakCacheMipmap) {
                addInteger(hash, minimum);
                addInteger(hash, maximum);
            }
            addInteger(hash, draft.notes.size());
            for (const auto &note : draft.notes) {
                addString(hash, note.clientRef);
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
                hash.addData(
                    QJsonDocument(note.phonemes.serialize()).toJson(QJsonDocument::Compact));
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
                    addInteger(hash, curve.values.size());
                    for (const auto value : curve.values)
                        addInteger(hash, value);
                    addInteger(hash, curve.nodes.size());
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
        return {TrackId(track.id()), track.name(),   control.gain(),
                control.pan(),       control.mute(), control.solo()};
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

    AudioAssetSnapshotDto audioAssetSnapshotDto(const AudioClip &clip) {
        return {
            .path = clip.path(),
            .pathInfo = clip.pathInfo(),
            .formatData = clip.workspace().value("diffscope.audio.formatData"),
            .sourceGeneration = clip.sourceGeneration(),
        };
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

    std::unique_ptr<Note> buildNote(const NoteDraftDto &draft, SingingClip *clip,
                                    QList<CreatedObjectRef> *createdObjects) {
        auto note = std::make_unique<Note>(clip);
        appendCreatedObject(createdObjects, draft.clientRef, ObjectKind::Note, note->id());
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
            for (const auto type : {Param::Original, Param::Edited, Param::Envelope}) {
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

    DocumentDraftDto documentDraftDto(const ProjectModelData &data,
                                      const LoopSettings &loopSettings) {
        DocumentDraftDto result;
        result.timeline = data.timeline;
        result.masterControl = data.masterControl;
        result.loopSettings = loopSettings;
        result.tracks.reserve(static_cast<qsizetype>(data.tracks.size()));
        for (const auto &track : data.tracks) {
            if (track)
                result.tracks.append(trackDraftDto(*track));
        }
        return result;
    }

    std::unique_ptr<Clip> buildClip(const ClipDraftDto &draft, const Track *targetTrack,
                                    const Timeline &timeline,
                                    QList<CreatedObjectRef> *createdObjects) {
        std::unique_ptr<Clip> result;
        if (draft.type == ClipDraftDto::Type::Audio) {
            auto audio = std::make_unique<AudioClip>();
            appendCreatedObject(createdObjects, draft.clientRef, ObjectKind::Clip, audio->id());
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
            appendCreatedObject(createdObjects, draft.clientRef, ObjectKind::Clip, singing->id());
            singing->setDefaultLanguage(draft.defaultLanguage);
            for (const auto &noteDraft : draft.notes) {
                auto note = buildNote(noteDraft, singing.get(), createdObjects);
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

    std::unique_ptr<Track> buildTrack(const TrackDraftDto &draft, const Timeline &timeline,
                                      QList<CreatedObjectRef> *createdObjects) {
        auto result = std::make_unique<Track>();
        appendCreatedObject(createdObjects, draft.clientRef, ObjectKind::Track, result->id());
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
            auto clip = buildClip(clipDraft, result.get(), timeline, createdObjects);
            result->insertClip(clip.release());
        }
        return result;
    }

    ProjectModelData buildProjectModelData(const DocumentDraftDto &draft,
                                           QList<CreatedObjectRef> *createdObjects) {
        ProjectModelData result;
        result.timeline = draft.timeline;
        result.masterControl = draft.masterControl;
        result.tracks.reserve(static_cast<size_t>(draft.tracks.size()));
        for (const auto &trackDraft : draft.tracks)
            result.tracks.push_back(buildTrack(trackDraft, draft.timeline, createdObjects));
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

    QByteArray fingerprint(const SingerInfo &singerInfo) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addSingerInfo(hash, singerInfo);
        return hash.result();
    }

    QByteArray fingerprint(const SpeakerInfo &speakerInfo) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addSpeakerInfo(hash, speakerInfo);
        return hash.result();
    }

    QByteArray fingerprint(const SpeakerMixModel::SpeakerMixData &speakerMixData) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addSpeakerMix(hash, speakerMixData);
        return hash.result();
    }

    QByteArray fingerprint(const TrackDraftDto &draft) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addString(hash, draft.clientRef);
        addString(hash, draft.name);
        addInteger(hash, draft.colorIndex);
        addDouble(hash, draft.gain);
        addDouble(hash, draft.pan);
        addInteger(hash, draft.mute);
        addInteger(hash, draft.solo);
        addString(hash, draft.defaultLanguage);
        addSingerInfo(hash, draft.singerInfo);
        addSpeakerInfo(hash, draft.speakerInfo);
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

    QByteArray fingerprint(const DocumentDraftDto &draft) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addInteger(hash, draft.timeline.tempos().size());
        for (const auto &tempo : draft.timeline.tempos()) {
            addInteger(hash, tempo.pos);
            addDouble(hash, tempo.value);
        }
        addInteger(hash, draft.timeline.timeSignatures().size());
        for (const auto &signature : draft.timeline.timeSignatures()) {
            addInteger(hash, signature.barIndex);
            addInteger(hash, signature.numerator);
            addInteger(hash, signature.denominator);
        }
        addDouble(hash, draft.masterControl.gain());
        addDouble(hash, draft.masterControl.pan());
        addInteger(hash, draft.masterControl.mute());
        addInteger(hash, draft.masterControl.solo());
        addInteger(hash, draft.tracks.size());
        for (const auto &track : draft.tracks)
            hash.addData(fingerprint(track));
        hash.addData(QJsonDocument(draft.loopSettings.serialize()).toJson(QJsonDocument::Compact));
        return hash.result();
    }

    QByteArray fingerprint(const BatchImportDraftDto &draft) {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        addInteger(hash, draft.timeline.tempos().size());
        for (const auto &tempo : draft.timeline.tempos()) {
            addInteger(hash, tempo.pos);
            addDouble(hash, tempo.value);
        }
        addInteger(hash, draft.timeline.timeSignatures().size());
        for (const auto &signature : draft.timeline.timeSignatures()) {
            addInteger(hash, signature.barIndex);
            addInteger(hash, signature.numerator);
            addInteger(hash, signature.denominator);
        }
        addInteger(hash, draft.items.size());
        for (const auto &item : draft.items) {
            addInteger(hash, item.existingTrackId.has_value());
            addInteger(hash, item.existingTrackId ? item.existingTrackId->value() : -1);
            hash.addData(fingerprint(item.newTrack));
            addInteger(hash, item.clips.size());
            for (const auto &clip : item.clips)
                addClipDraft(hash, clip);
        }
        return hash.result();
    }

    AutomationResult<AutomationUnit> validate(const ClipDraftDto &draft) {
        const auto &properties = draft.properties;
        if (properties.start + properties.clipStart < 0 || properties.length < 0 ||
            properties.clipStart < 0 || properties.clipLen < 0 ||
            properties.clipStart + properties.clipLen > properties.length ||
            !std::isfinite(properties.gain) || !std::isfinite(properties.trimStartMs) ||
            !std::isfinite(properties.playLengthMs) ||
            !std::isfinite(properties.materialLengthMs)) {
            return AutomationError::invalidArgument(
                QStringLiteral("clip.properties"),
                QStringLiteral("Clip geometry, timing, or gain is invalid"));
        }
        if (draft.type == ClipDraftDto::Type::Audio) {
            if (draft.audioPath.isEmpty()) {
                return AutomationError::invalidArgument(QStringLiteral("clip.audio_path"),
                                                        QStringLiteral("Audio path is empty"));
            }
            if (draft.hasRealTimeAnchor &&
                (properties.trimStartMs < 0 || properties.playLengthMs < 0 ||
                 properties.materialLengthMs < 0 ||
                 properties.trimStartMs + properties.playLengthMs > properties.materialLengthMs)) {
                return AutomationError::invalidArgument(
                    QStringLiteral("clip.audio_timing"),
                    QStringLiteral("Audio real-time anchor is invalid"));
            }
            QStringList clientRefs;
            collectClientRefs(draft, clientRefs);
            return validateUniqueClientRefs(clientRefs);
        }
        for (const auto &note : draft.notes) {
            if (note.localStart < 0 || note.length <= 0 || note.keyIndex < 0 ||
                note.keyIndex > 127) {
                return AutomationError::invalidArgument(
                    QStringLiteral("clip.notes"),
                    QStringLiteral("Note geometry or key is invalid"));
            }
        }
        for (const auto &parameter : draft.params) {
            if (parameter.name < ParamInfo::Pitch || parameter.name > ParamInfo::ToneShift ||
                (parameter.type != Param::Original && parameter.type != Param::Edited &&
                 parameter.type != Param::Envelope)) {
                return AutomationError::invalidArgument(
                    QStringLiteral("clip.parameters"),
                    QStringLiteral("Parameter name or type is unsupported"));
            }
            for (const auto &curve : parameter.curves) {
                if (curve.type == CurveDraftDto::Type::Draw && curve.step <= 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("clip.parameters.curves.step"),
                        QStringLiteral("Curve step must be positive"));
                }
            }
        }
        QStringList clientRefs;
        collectClientRefs(draft, clientRefs);
        return validateUniqueClientRefs(clientRefs);
    }

    AutomationResult<AutomationUnit> validate(const TrackDraftDto &draft) {
        if (!std::isfinite(draft.gain) || !std::isfinite(draft.pan) || draft.colorIndex < 0 ||
            draft.colorIndex >= AutomationWire::TrackPaletteColorCount) {
            return AutomationError::invalidArgument(QStringLiteral("track"),
                                                    QStringLiteral("Track properties are invalid"));
        }
        for (const auto &clip : draft.clips) {
            auto result = validate(clip);
            if (!result)
                return result;
        }
        QStringList clientRefs;
        collectClientRefs(draft, clientRefs);
        return validateUniqueClientRefs(clientRefs);
    }

    AutomationResult<AutomationUnit> validate(const DocumentDraftDto &draft) {
        if (draft.timeline.tempos().isEmpty() || draft.timeline.timeSignatures().isEmpty()) {
            return AutomationError::invalidArgument(QStringLiteral("document.timeline"),
                                                    QStringLiteral("Timeline is incomplete"));
        }
        for (const auto &tempo : draft.timeline.tempos()) {
            if (tempo.pos < 0 || !std::isfinite(tempo.value) || tempo.value <= 0) {
                return AutomationError::invalidArgument(QStringLiteral("document.tempos"),
                                                        QStringLiteral("Tempo is invalid"));
            }
        }
        for (const auto &signature : draft.timeline.timeSignatures()) {
            if (signature.barIndex < 0 || signature.numerator <= 0 || signature.denominator <= 0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("document.time_signatures"),
                    QStringLiteral("Time signature is invalid"));
            }
        }
        if (!std::isfinite(draft.masterControl.gain()) ||
            !std::isfinite(draft.masterControl.pan())) {
            return AutomationError::invalidArgument(QStringLiteral("document.master_control"),
                                                    QStringLiteral("Master control is invalid"));
        }
        for (const auto &track : draft.tracks) {
            auto result = validate(track);
            if (!result)
                return result;
        }
        QStringList clientRefs;
        for (const auto &track : draft.tracks)
            collectClientRefs(track, clientRefs);
        return validateUniqueClientRefs(clientRefs);
    }

    AutomationResult<AutomationUnit> validateClientRefs(const QList<NoteDraftDto> &notes) {
        QStringList clientRefs;
        clientRefs.reserve(notes.size());
        for (const auto &note : notes)
            collectClientRefs(note, clientRefs);
        return validateUniqueClientRefs(clientRefs);
    }

    AutomationResult<AutomationUnit> validateClientRefs(const QList<ClipInsertDto> &clips) {
        QStringList clientRefs;
        for (const auto &item : clips)
            collectClientRefs(item.clip, clientRefs);
        return validateUniqueClientRefs(clientRefs);
    }

    AutomationResult<AutomationUnit> validateClientRefs(const BatchImportDraftDto &batch) {
        QStringList clientRefs;
        for (const auto &item : batch.items) {
            if (!item.existingTrackId)
                collectClientRefs(item.newTrack, clientRefs, false);
            for (const auto &clip : item.clips)
                collectClientRefs(clip, clientRefs);
        }
        return validateUniqueClientRefs(clientRefs);
    }

} // namespace Automation
