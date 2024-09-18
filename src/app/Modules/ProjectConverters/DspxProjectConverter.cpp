//
// Created by hrukalive on 2/7/24.
//

#include "DspxProjectConverter.h"

#include "Model/AppModel/AnchorCurve.h"

#include <QMessageBox>

#include "opendspx/qdspxmodel.h"
#include "Model/AppModel/Track.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Params.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/SingingClip.h"

bool DspxProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                                ImportMode mode) {
    auto decodeCurves = [&](const QList<QDspx::ParamCurveRef> &dspxCurveRefs) {
        QVector<Curve *> curves;
        for (const QDspx::ParamCurveRef &dspxCurveRef : dspxCurveRefs) {
            if (dspxCurveRef->type == QDspx::ParamCurve::Type::Free) {
                const auto castCurveRef = dspxCurveRef.dynamicCast<QDspx::ParamFree>();
                const auto curve = new DrawCurve;
                curve->start = castCurveRef->start;
                curve->step = castCurveRef->step;
                curve->setValues(castCurveRef->values);
                curves.append(curve);
            } else if (dspxCurveRef->type == QDspx::ParamCurve::Type::Anchor) {
                const auto castCurveRef = dspxCurveRef.dynamicCast<QDspx::ParamAnchor>();
                const auto curve = new AnchorCurve;
                curve->start = castCurveRef->start;
                for (const auto &dspxNode : castCurveRef->nodes) {
                    const auto node = new AnchorNode(dspxNode.x, dspxNode.y);
                    node->setInterpMode(AnchorNode::None);
                    if (dspxNode.interp == QDspx::AnchorPoint::Interpolation::Linear) {
                        node->setInterpMode(AnchorNode::Linear);
                    } else if (dspxNode.interp == QDspx::AnchorPoint::Interpolation::Hermite) {
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

    auto decodeSingingParam = [&](const QDspx::ParamInfo &dspxParam) {
        Param param;
        param.setCurves(Param::Original, decodeCurves(dspxParam.org));
        param.setCurves(Param::Edited, decodeCurves(dspxParam.edited));
        param.setCurves(Param::Envelope, decodeCurves(dspxParam.envelope));
        return param;
    };

    auto decodeSingingParams = [&](const QDspx::SingleParam &dspxParams) {
        ParamInfo params;
        params.pitch = std::move(decodeSingingParam(dspxParams.pitch));
        params.energy = std::move(decodeSingingParam(dspxParams.energy));
        params.tension = std::move(decodeSingingParam(dspxParams.tension));
        params.breathiness = std::move(decodeSingingParam(dspxParams.breathiness));
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
    auto decodeNotes = [&](const QList<QDspx::Note> &dspxNotes, int offset) {
        QList<Note *> notes;
        for (const QDspx::Note &dspxNote : dspxNotes) {
            const auto note = new Note;
            note->setRStart(dspxNote.pos - offset);
            note->setLength(dspxNote.length);
            note->setKeyIndex(dspxNote.keyNum);
            note->setLyric(dspxNote.lyric);
            note->setLanguage(dspxNote.language);
            note->setPronunciation(
                Pronunciation(dspxNote.pronunciation.org, dspxNote.pronunciation.edited));
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
                const auto castClip = dspxClip.dynamicCast<QDspx::SingingClip>();
                const auto clip = new SingingClip;
                clip->setName(castClip->name);
                clip->setStart(castClip->time.start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                auto notes = decodeNotes(castClip->notes, castClip->time.start);
                for (auto &note : notes)
                    clip->insertNote(note);
                clip->params = std::move(decodeSingingParams(castClip->params));
                track->insertClip(clip);
            } else if (dspxClip->type == QDspx::Clip::Type::Audio) {
                const auto castClip = dspxClip.dynamicCast<QDspx::AudioClip>();
                const auto clip = new AudioClip;
                clip->setName(castClip->name);
                clip->setStart(castClip->time.start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                clip->setPath(castClip->path);
                track->insertClip(clip);
            }
        }
    };

    auto decodeTracks = [&](const QList<QDspx::Track> &dspxTracks, AppModel *model) {
        int i = 0;
        for (const auto &dspxTrack : dspxTracks) {
            const auto track = new Track;
            auto trackControl = TrackControl();
            trackControl.setGain(dspxTrack.control.gain);
            trackControl.setPan(dspxTrack.control.pan);
            trackControl.setMute(dspxTrack.control.mute);
            trackControl.setSolo(dspxTrack.control.solo);
            track->setName(dspxTrack.name);
            track->setControl(trackControl);
            decodeClips(dspxTrack.clips, track);
            model->insertTrack(track, i);
            i++;
        }
    };

    QDspxModel dspxModel;
    const auto returnCode = dspxModel.load(path);
    if (returnCode.type == QDspx::Result::Success) {
        const auto timeline = dspxModel.content.timeline;
        model->setTimeSignature(
            TimeSignature(timeline.timeSignatures[0].num, timeline.timeSignatures[0].den));
        model->setTempo(timeline.tempos[0].value);
        decodeTracks(dspxModel.content.tracks, model);
        return true;
    } else {
        errMsg = QString("Failed to load project file.\r\npath: %1\r\ntype: %2 code: %3")
                     .arg(path)
                     .arg(returnCode.type)
                     .arg(returnCode.code);
        return false;
    }
}

bool DspxProjectConverter::save(const QString &path, AppModel *model, QString &errMsg) {

    auto encodeCurves = [&](const QList<Curve *> &dsCurves, QList<QDspx::ParamCurveRef> &curves) {
        for (const auto &dsCurve : dsCurves) {
            if (dsCurve->type() == Curve::CurveType::Draw) {
                const auto castCurve = dynamic_cast<DrawCurve *>(dsCurve);
                const auto curve = QDspx::ParamFreeRef::create();
                curve->start = castCurve->start;
                curve->step = castCurve->step;
                for (const auto v : castCurve->values()) {
                    curve->values.append(v);
                }
                curves.append(curve);
            } else if (dsCurve->type() == Curve::CurveType::Anchor) {
                const auto castCurve = dynamic_cast<AnchorCurve *>(dsCurve);
                const auto curve = QDspx::ParamAnchorRef::create();
                curve->start = dsCurve->start;
                for (const auto dsNode : castCurve->nodes()) {
                    QDspx::AnchorPoint node;
                    node.x = dsNode->pos();
                    node.y = dsNode->value();
                    if (dsNode->interpMode() == AnchorNode::None) {
                        node.interp = QDspx::AnchorPoint::Interpolation::None;
                    } else if (dsNode->interpMode() == AnchorNode::Linear) {
                        node.interp = QDspx::AnchorPoint::Interpolation::Linear;
                    } else if (dsNode->interpMode() == AnchorNode::Hermite) {
                        node.interp = QDspx::AnchorPoint::Interpolation::Hermite;
                    } /*else if (dsNode->interpMode() == DsAnchorNode::Cubic) {
                        node.interp = QDspx::AnchorPoint::Interpolation::Cubic;
                    }*/
                    curve->nodes.append(node);
                }
                curves.append(curve);
            }
        }
    };

    auto encodeSingingParam = [&](const Param &dsParam, QDspx::ParamInfo &param) {
        encodeCurves(dsParam.curves(Param::Original), param.org);
        encodeCurves(dsParam.curves(Param::Edited), param.edited);
        encodeCurves(dsParam.curves(Param::Envelope), param.envelope);
    };

    auto encodeSingingParams = [&](const ParamInfo &dsParams, QDspx::SingleParam &params) {
        encodeSingingParam(dsParams.pitch, params.pitch);
        encodeSingingParam(dsParams.energy, params.energy);
        encodeSingingParam(dsParams.tension, params.tension);
        encodeSingingParam(dsParams.breathiness, params.breathiness);
    };

    // auto encodePhonemes = [&](const QList<Phoneme> &dsPhonemes, QList<QDspx::Phoneme> &phonemes) {
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
            note.pos = dsNote->start();
            note.length = dsNote->length();
            note.keyNum = dsNote->keyIndex();
            note.lyric = dsNote->lyric();
            note.language = dsNote->language();
            note.pronunciation.org = dsNote->pronunciation().original;
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
                singClip->time.start = clip->start();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->control.gain = clip->gain();
                singClip->control.mute = clip->mute();
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
                track.clips.append(audioClipRef);
            }
        }
    };

    auto encodeTracks = [&](const AppModel *model, QDspx::Model &dspx) {
        for (const auto dsTrack : model->tracks()) {
            QDspx::Track track;
            track.name = dsTrack->name();
            track.control.gain = dsTrack->control().gain();
            track.control.pan = dsTrack->control().pan();
            track.control.mute = dsTrack->control().mute();
            track.control.solo = dsTrack->control().solo();
            encodeClips(dsTrack, track);
            dspx.content.tracks.append(track);
        }
    };

    QDspx::Model dspxModel;
    auto &timeline = dspxModel.content.timeline;
    timeline.tempos.append(QDspx::Tempo(0, model->tempo()));
    timeline.timeSignatures.append(QDspx::TimeSignature(0, model->timeSignature().numerator,
                                                        model->timeSignature().denominator));

    encodeTracks(model, dspxModel);
    const auto returnCode = dspxModel.save(path);

    if (returnCode.type != QDspx::Result::Success) {
        QMessageBox::warning(
            nullptr, "Warning",
            QString("Failed to save project file.\r\npath: %1\r\ntype: %2 code: %3")
                .arg(path)
                .arg(returnCode.type)
                .arg(returnCode.code));
        return false;
    }
    return true;
}