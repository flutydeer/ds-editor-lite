//
// Created by hrukalive on 2/7/24.
//

#include "DspxProjectConverter.h"

#include "Model/AppModel/AnchorCurve.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppStatus/AppStatus.h"

#include <QMessageBox>

#include <opendspx/model.h>
#include <opendspxserializer/serializer.h>

#include "Model/AppModel/Track.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Params.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/SingingClip.h"

#include <QFile>

bool DspxProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                                ImportMode mode) {
    auto decodeCurves = [&](const QList<QDspx::ParamCurveRef> &dspxCurveRefs, const int offset) {
        QVector<Curve *> curves;
        for (const QDspx::ParamCurveRef &dspxCurveRef : dspxCurveRefs) {
            if (dspxCurveRef->type == QDspx::ParamCurve::Type::Free) {
                const auto castCurveRef = dspxCurveRef.staticCast<QDspx::ParamCurveFree>();
                const auto curve = new DrawCurve;
                curve->setLocalStart(castCurveRef->start - offset);
                curve->step = castCurveRef->step;
                curve->setValues(castCurveRef->values);
                curves.append(curve);
            } else if (dspxCurveRef->type == QDspx::ParamCurve::Type::Anchor) {
                const auto castCurveRef = dspxCurveRef.staticCast<QDspx::ParamCurveAnchor>();
                const auto curve = new AnchorCurve;
                curve->setLocalStart(castCurveRef->start - offset);
                for (const auto &[interp, x, y] : castCurveRef->nodes) {
                    const auto node = new AnchorNode(x, y);
                    node->setInterpMode(AnchorNode::None);
                    if (interp == QDspx::AnchorNode::Interpolation::Linear) {
                        node->setInterpMode(AnchorNode::Linear);
                    } else if (interp == QDspx::AnchorNode::Interpolation::Hermite) {
                        node->setInterpMode(AnchorNode::Hermite);
                    } /*else if (dspxNode.interp == QDspx::AnchorPoint::Interpolation::Cubic) {
                        node->setInterpMode(DsAnchorNode::Cubic);
                    }*/
                    curve->insertNode(node);
                }
                curves.append(curve);
            }
        }
        return curves;
    };

    auto decodeSingingParam = [&](const QDspx::Param &dspxParam, const int offset,
                                  SingingClip *clip) {
        Param param;
        param.setCurves(Param::Original, decodeCurves(dspxParam.original, offset), clip);
        param.setCurves(Param::Edited, decodeCurves(dspxParam.edited, offset), clip);
        param.setCurves(Param::Envelope, decodeCurves(dspxParam.transform, offset), clip);
        return param;
    };

    auto decodeSingingParams = [&](const QDspx::Params &dspxParams, const int offset,
                                   SingingClip *clip) {
        ParamInfo params(clip);
        params.pitch = std::move(decodeSingingParam(dspxParams["pitch"], offset, clip));
        params.expressiveness =
            std::move(decodeSingingParam(dspxParams["expressiveness"], offset, clip));
        params.energy = std::move(decodeSingingParam(dspxParams["energy"], offset, clip));
        params.breathiness = std::move(decodeSingingParam(dspxParams["breathiness"], offset, clip));
        params.voicing = std::move(decodeSingingParam(dspxParams["voicing"], offset, clip));
        params.tension = std::move(decodeSingingParam(dspxParams["tension"], offset, clip));
        params.gender = std::move(decodeSingingParam(dspxParams["gender"], offset, clip));
        params.velocity = std::move(decodeSingingParam(dspxParams["velocity"], offset, clip));
        return params;
    };

    // auto decodePhonemes = [&](const QList<QDspx::Phoneme> &dspxPhonemes) {
    //     QList<Phoneme> phonemes;
    //     for (const QDspx::Phoneme &dspxPhoneme : dspxPhonemes) {
    //         Phoneme phoneme(Phoneme::PhonemeType::Normal, dspxPhoneme.token, dspxPhoneme.start);
    //         if (dspxPhoneme.type == QDspx::Phoneme::Type::Ahead) {
    //             phoneme.type = Phoneme::PhonemeType::Ahead;
    //         } else if (dspxPhoneme.type == QDspx::Phoneme::Type::Final) {
    //             phoneme.type = Phoneme::PhonemeType::Final;
    //         }
    //         phonemes.append(phoneme);
    //     }
    //     return phonemes;
    // };
    auto decodeNotes = [&](const QList<QDspx::Note> &dspxNotes, const int offset) {
        QList<Note *> notes;
        for (const QDspx::Note &dspxNote : dspxNotes) {
            const auto note = new Note;
            note->setLocalStart(dspxNote.pos - offset);
            note->setLength(dspxNote.length);
            note->setKeyIndex(dspxNote.keyNum);
            note->setCentShift(dspxNote.centShift);
            note->setLyric(dspxNote.lyric);
            note->setLanguage(dspxNote.language);
            note->setPronunciation(
                Pronunciation(dspxNote.pronunciation.original, dspxNote.pronunciation.edited));
            note->setWorkspace(dspxNote.workspace);
            // note->setPhonemeInfo(Note::Original, decodePhonemes(dspxNote.phonemes.org));
            // note->setPhonemeInfo(Note::Edited, decodePhonemes(dspxNote.phonemes.edited));
            notes.append(note);
        }
        return notes;
    };
    auto decodeClips = [&](const QList<QDspx::ClipRef> &dspxClips, Track *track) {
        for (const auto &dspxClip : dspxClips) {
            if (dspxClip->type == QDspx::Clip::Type::Singing) {
                const auto castClip = dspxClip.staticCast<QDspx::SingingClip>();
                const auto clip = new SingingClip;
                clip->setName(castClip->name);
                // TODO: language
                // clip->setDefaultLanguage(castClip->language);
                clip->setStart(castClip->time.start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                auto notes = decodeNotes(castClip->notes, castClip->time.start);
                for (const auto &note : notes)
                    clip->insertNote(note);
                clip->params =
                    std::move(decodeSingingParams(castClip->params, castClip->time.start, clip));
                clip->workspace() = castClip->workspace;
                track->insertClip(clip);
            } else if (dspxClip->type == QDspx::Clip::Type::Audio) {
                const auto castClip = dspxClip.staticCast<QDspx::AudioClip>();
                const auto clip = new AudioClip;
                clip->setName(castClip->name);
                clip->setStart(castClip->time.start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                clip->setPath(castClip->path);
                clip->workspace() = castClip->workspace;
                track->insertClip(clip);
            }
        }
    };

    auto decodeTracks = [&](const QList<QDspx::Track> &dspxTracks, AppModel *model_) {
        int i = 0;
        for (const auto &dspxTrack : dspxTracks) {
            const auto track = new Track;
            auto trackControl = TrackControl();
            trackControl.setGain(dspxTrack.control.gain);
            trackControl.setPan(dspxTrack.control.pan);
            trackControl.setMute(dspxTrack.control.mute);
            trackControl.setSolo(dspxTrack.control.solo);
            track->setName(dspxTrack.name);
            // TODO: lanauge
            // track->setDefaultLanguage(dspxTrack.language);
            track->setControl(trackControl);
            decodeClips(dspxTrack.clips, track);
            model_->insertTrack(track, i);
            i++;
        }
    };

    struct LoadResult {
        enum Type { Success, Failure } type;

        QDspx::SerializationErrorList errors;
    };

    auto loadModel = [](const QString &filePath, QDspx::Model &outModel) -> LoadResult {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return LoadResult{LoadResult::Failure, {}};

        const QByteArray data = file.readAll();
        QDspx::SerializationErrorList errors;
        outModel = QDspx::Serializer::deserialize(data, errors, QDspx::Serializer::CheckError);

        LoadResult result;
        if (errors.containsFatal()) {
            result.type = LoadResult::Failure;
        } else {
            result.type = LoadResult::Success;
        }
        result.errors = errors;
        return result;
    };

    QDspx::Model dspxModel;
    auto [type, errors] = loadModel(path, dspxModel);
    if (type == LoadResult::Success) {
        // dspxModel.content.global.centShift
        // TODO: where should I use centShift in the editor?
        const auto timeline = dspxModel.content.timeline;
        model->setTimeSignature(TimeSignature(timeline.timeSignatures[0].numerator,
                                              timeline.timeSignatures[0].denominator));
        model->setTempo(timeline.tempos[0].value);
        decodeTracks(dspxModel.content.tracks, model);

        // Load loop settings from workspace
        const auto &workspace = dspxModel.content.workspace;
        if (workspace.contains("loop")) {
            LoopSettings loopSettings;
            loopSettings.deserialize(workspace["loop"]);
            appStatus->loopSettings.set(loopSettings);
        } else {
            appStatus->loopSettings.set(LoopSettings());
        }

        return true;
    }

    QString errorDetails;
    for (const auto &err : errors)
        errorDetails += QString("Error type: %1\n").arg(err->type());

    errMsg = QString("Failed to load project file.\r\npath: %1\r\nerrors: %2")
                 .arg(path)
                 .arg(errorDetails);
    return false;
}

bool DspxProjectConverter::save(const QString &path, AppModel *model, QString &errMsg) {

    auto encodeCurves = [&](const QList<Curve *> &dsCurves, QList<QDspx::ParamCurveRef> &curves) {
        for (const auto &dsCurve : dsCurves) {
            if (dsCurve->type() == Curve::CurveType::Draw) {
                const auto castCurve = dynamic_cast<DrawCurve *>(dsCurve);
                const auto curve = QDspx::ParamCurveFreeRef::create();
                curve->start = castCurve->globalStart();
                curve->step = castCurve->step;
                for (const auto v : castCurve->values()) {
                    curve->values.append(v);
                }
                curves.append(curve);
            } else if (dsCurve->type() == Curve::CurveType::Anchor) {
                const auto castCurve = dynamic_cast<AnchorCurve *>(dsCurve);
                const auto curve = QDspx::ParamCurveAnchorRef::create();
                curve->start = dsCurve->globalStart();
                for (const auto dsNode : castCurve->nodes()) {
                    QDspx::AnchorNode node;
                    node.x = dsNode->pos();
                    node.y = dsNode->value();
                    if (dsNode->interpMode() == AnchorNode::None) {
                        node.interp = QDspx::AnchorNode::Interpolation::None;
                    } else if (dsNode->interpMode() == AnchorNode::Linear) {
                        node.interp = QDspx::AnchorNode::Interpolation::Linear;
                    } else if (dsNode->interpMode() == AnchorNode::Hermite) {
                        node.interp = QDspx::AnchorNode::Interpolation::Hermite;
                    } /*else if (dsNode->interpMode() == DsAnchorNode::Cubic) {
                        node.interp = QDspx::AnchorNode::Interpolation::Cubic;
                    }*/
                    curve->nodes.append(node);
                }
                curves.append(curve);
            }
        }
    };

    auto encodeSingingParam = [&](const Param &dsParam, QDspx::Param &param) {
        encodeCurves(dsParam.curves(Param::Original), param.original);
        encodeCurves(dsParam.curves(Param::Edited), param.edited);
        encodeCurves(dsParam.curves(Param::Envelope), param.transform);
    };

    auto encodeSingingParams = [&](const ParamInfo &dsParams, QDspx::Params &params) {
        encodeSingingParam(dsParams.pitch, params["pitch"]);
        encodeSingingParam(dsParams.expressiveness, params["expressiveness"]);
        encodeSingingParam(dsParams.energy, params["energy"]);
        encodeSingingParam(dsParams.breathiness, params["breathiness"]);
        encodeSingingParam(dsParams.voicing, params["voicing"]);
        encodeSingingParam(dsParams.tension, params["tension"]);
        encodeSingingParam(dsParams.gender, params["gender"]);
        encodeSingingParam(dsParams.velocity, params["velocity"]);
    };

    // auto encodePhonemes = [&](const QList<Phoneme> &dsPhonemes, QList<QDspx::Phoneme> &phonemes)
    // {
    //     for (const auto &dsPhoneme : dsPhonemes) {
    //         QDspx::Phoneme phoneme;
    //         phoneme.start = dsPhoneme.start;
    //         phoneme.token = dsPhoneme.name;
    //         if (dsPhoneme.type == Phoneme::PhonemeType::Ahead) {
    //             phoneme.type = QDspx::Phoneme::Type::Ahead;
    //         } else if (dsPhoneme.type == Phoneme::PhonemeType::Final) {
    //             phoneme.type = QDspx::Phoneme::Type::Final;
    //         } else {
    //             phoneme.type = QDspx::Phoneme::Type::Normal;
    //         }
    //         phonemes.append(phoneme);
    //     }
    // };

    auto encodeNotes = [&](const OverlappableSerialList<Note> &dsNotes, QList<QDspx::Note> &notes) {
        for (const auto dsNote : dsNotes) {
            QDspx::Note note;
            note.pos = dsNote->globalStart();
            note.length = dsNote->length();
            note.keyNum = dsNote->keyIndex();
            note.centShift = dsNote->centShift();
            note.lyric = dsNote->lyric();
            note.language = dsNote->language();
            note.pronunciation.original = dsNote->pronunciation().original;
            note.pronunciation.edited = dsNote->pronunciation().edited;
            // encodePhonemes(dsNote->phonemeInfo().original, note.phonemes.org);
            // encodePhonemes(dsNote->phonemeInfo().edited, note.phonemes.edited);
            note.workspace = dsNote->workspace();
            notes.append(note);
        }
    };

    auto encodeClips = [&](const Track *dsTrack, QDspx::Track &track) {
        for (const auto clip : dsTrack->clips()) {
            if (clip->clipType() == Clip::Singing) {
                const auto singingClip = dynamic_cast<SingingClip *>(clip);
                auto singClip = QDspx::SingingClipRef::create();
                singClip->name = clip->name();
                // TODO: language
                // singClip->language = singingClip->defaultLanguage();
                singClip->time.start = clip->start();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->control.gain = clip->gain();
                singClip->control.mute = clip->mute();
                singClip->workspace = clip->workspace();
                encodeNotes(singingClip->notes(), singClip->notes);
                encodeSingingParams(singingClip->params, singClip->params);
                track.clips.append(singClip);
            } else if (clip->clipType() == Clip::Audio) {
                const auto audioClip = dynamic_cast<AudioClip *>(clip);
                auto audioClipRef = QDspx::AudioClipRef::create();
                audioClipRef->name = clip->name();
                audioClipRef->time.start = clip->start();
                audioClipRef->time.clipStart = clip->clipStart();
                audioClipRef->time.length = clip->length();
                audioClipRef->time.clipLen = clip->clipLen();
                audioClipRef->control.gain = clip->gain();
                audioClipRef->control.mute = clip->mute();
                audioClipRef->path = audioClip->path();
                audioClipRef->workspace = audioClip->workspace();
                track.clips.append(audioClipRef);
            }
        }
    };

    auto encodeTracks = [&](const AppModel *model_, QDspx::Model &dspx) {
        for (const auto dsTrack : model_->tracks()) {
            QDspx::Track track;
            track.name = dsTrack->name();
            // TODO: language
            // track.language = dsTrack->defaultLanguage();
            // track.g2pId = dsTrack->defaultG2pId();
            track.control.gain = dsTrack->control().gain();
            track.control.pan = dsTrack->control().pan();
            track.control.mute = dsTrack->control().mute();
            track.control.solo = dsTrack->control().solo();
            encodeClips(dsTrack, track);
            dspx.content.tracks.append(track);
        }
    };

    QDspx::Model dspxModel;
    dspxModel.content.global.centShift = 0; // TODO: where should I use centShift in the editor?
    auto &timeline = dspxModel.content.timeline;
    timeline.tempos.append(QDspx::Tempo(0, model->tempo()));
    timeline.timeSignatures.append(QDspx::TimeSignature(0, model->timeSignature().numerator,
                                                        model->timeSignature().denominator));

    encodeTracks(model, dspxModel);

    const auto loopSettings = appStatus->loopSettings.get();
    dspxModel.content.workspace["loop"] = loopSettings.serialize();

    auto saveModelToFile = [](const QDspx::Model &model_, const QString &filePath,
                              QString &msg) -> bool {
        QDspx::SerializationErrorList errors;
        const QByteArray jsonData = QDspx::Serializer::serialize(
            model_, errors, QDspx::Serializer::FailFast | QDspx::Serializer::CheckError);

        if (!errors.isEmpty()) {
            msg += "Serialization errors occurred:";
            for (const auto &err : errors)
                msg += "  Error type:" + std::to_string(err->type());
            return false;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            msg += "Failed to open file for writing:" + filePath;
            return false;
        }

        const qint64 written = file.write(jsonData);
        file.close();

        if (written != jsonData.size()) {
            msg += "Failed to write all data to file:" + filePath;
            return false;
        }

        return true;
    };

    QString errorMsg;
    const auto returnCode = saveModelToFile(dspxModel, path, errorMsg);

    if (!returnCode) {
        QMessageBox::warning(nullptr, "Warning",
                             QString("Failed to save project file.\r\npath: %1\r\nError: %2")
                                 .arg(path)
                                 .arg(errorMsg));
        return false;
    }
    return true;
}