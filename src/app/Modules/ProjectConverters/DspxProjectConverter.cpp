//
// Created by hrukalive on 2/7/24.
//

#include "DspxProjectConverter.h"

#include "Model/AppModel/AnchorCurve.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppStatus/AppStatus.h"

#include <opendspx/model.h>
#include <opendspxserializer/serializer.h>

#include "Model/AppModel/Track.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/Params.h"
#include "Model/AppModel/Curve.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/SingingClip.h"

#include <QFile>

namespace {
    class JsonUtils {
    public:
        static nlohmann::json fromQJsonValue(const QJsonValue &v) {
            if(v.isString())
                return v.toString().toStdString();

            if(v.isBool())
                return v.toBool();

            if(v.isDouble())
                return v.toDouble();

            if(v.isArray()) {
                nlohmann::json ret = nlohmann::json::array();
                std::ranges::transform(v.toArray(), std::back_inserter(ret), &JsonUtils::fromQJsonValue);
                return ret;
            }

            if(v.isObject()) {
                nlohmann::json ret = nlohmann::json::object();
                for (auto [key, value] : v.toObject().asKeyValueRange()) {
                    ret[key.toString().toStdString()] = fromQJsonValue(value);
                }
                return ret;
            }

            return {};
        }


        static QJsonValue toQJsonValue(const nlohmann::json &v) {
            if(v.is_string())
                return QString::fromStdString(v.get<std::string>());

            if(v.is_boolean())
                return v.get<bool>();

            if(v.is_number())
                return v.get<double>();

            if(v.is_array()) {
                QJsonArray ret;
                std::ranges::transform(v, std::back_inserter(ret), &JsonUtils::toQJsonValue);
                return ret;
            }

            if(v.is_object()) {
                QJsonObject ret;
                for (auto it = v.begin(); it != v.end(); ++it) {
                    ret[it.key().c_str()] = toQJsonValue(it.value());
                }
                return ret;
            }

            return {};
        }
    };
}

bool DspxProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                                ImportMode mode) {
    auto decodeCurves = [&](const std::vector<opendspx::ParamCurveRef> &dspxCurveRefs, const int offset) {
        QVector<Curve *> curves;
        for (const opendspx::ParamCurveRef &dspxCurveRef : dspxCurveRefs) {
            if (dspxCurveRef->type == opendspx::ParamCurve::Type::Free) {
                const auto castCurveRef = std::static_pointer_cast<opendspx::ParamCurveFree>(dspxCurveRef);
                const auto curve = new DrawCurve;
                curve->setLocalStart(castCurveRef->start - offset);
                curve->step = castCurveRef->step;
                curve->setValues(QList(castCurveRef->values.begin(), castCurveRef->values.end()));
                curves.append(curve);
            } else if (dspxCurveRef->type == opendspx::ParamCurve::Type::Anchor) {
                const auto castCurveRef = std::static_pointer_cast<opendspx::ParamCurveAnchor>(dspxCurveRef);
                const auto curve = new AnchorCurve;
                curve->setLocalStart(castCurveRef->start - offset);
                for (const auto &[interp, x, y] : castCurveRef->nodes) {
                    const auto node = new AnchorNode(x, y);
                    node->setInterpMode(AnchorNode::None);
                    if (interp == opendspx::AnchorNode::Interpolation::Linear) {
                        node->setInterpMode(AnchorNode::Linear);
                    } else if (interp == opendspx::AnchorNode::Interpolation::Hermite) {
                        node->setInterpMode(AnchorNode::Hermite);
                    } /*else if (dspxNode.interp == opendspx::AnchorPoint::Interpolation::Cubic) {
                        node->setInterpMode(DsAnchorNode::Cubic);
                    }*/
                    curve->insertNode(node);
                }
                curves.append(curve);
            }
        }
        return curves;
    };

    auto decodeSingingParam = [&](const opendspx::Param &dspxParam, const int offset,
                                  SingingClip *clip) {
        Param param;
        param.setCurves(Param::Original, decodeCurves(dspxParam.original, offset), clip);
        param.setCurves(Param::Edited, decodeCurves(dspxParam.edited, offset), clip);
        param.setCurves(Param::Envelope, decodeCurves(dspxParam.transform, offset), clip);
        return param;
    };

    auto decodeSingingParams = [&](const opendspx::Params &dspxParams, const int offset,
                                   SingingClip *clip) {
        ParamInfo params(clip);
        auto dspxParams_ = dspxParams;
        params.pitch = std::move(decodeSingingParam(dspxParams_["pitch"], offset, clip));
        params.expressiveness =
            std::move(decodeSingingParam(dspxParams_["expressiveness"], offset, clip));
        params.energy = std::move(decodeSingingParam(dspxParams_["energy"], offset, clip));
        params.breathiness = std::move(decodeSingingParam(dspxParams_["breathiness"], offset, clip));
        params.voicing = std::move(decodeSingingParam(dspxParams_["voicing"], offset, clip));
        params.tension = std::move(decodeSingingParam(dspxParams_["tension"], offset, clip));
        params.gender = std::move(decodeSingingParam(dspxParams_["gender"], offset, clip));
        params.velocity = std::move(decodeSingingParam(dspxParams_["velocity"], offset, clip));
        return params;
    };

    // auto decodePhonemes = [&](const QList<opendspx::Phoneme> &dspxPhonemes) {
    //     QList<Phoneme> phonemes;
    //     for (const opendspx::Phoneme &dspxPhoneme : dspxPhonemes) {
    //         Phoneme phoneme(Phoneme::PhonemeType::Normal, dspxPhoneme.token, dspxPhoneme.start);
    //         if (dspxPhoneme.type == opendspx::Phoneme::Type::Ahead) {
    //             phoneme.type = Phoneme::PhonemeType::Ahead;
    //         } else if (dspxPhoneme.type == opendspx::Phoneme::Type::Final) {
    //             phoneme.type = Phoneme::PhonemeType::Final;
    //         }
    //         phonemes.append(phoneme);
    //     }
    //     return phonemes;
    // };
    auto decodeNotes = [&](const std::vector<opendspx::Note> &dspxNotes, const int offset) {
        QList<Note *> notes;
        for (const opendspx::Note &dspxNote : dspxNotes) {
            const auto note = new Note;
            note->setLocalStart(dspxNote.pos - offset);
            note->setLength(dspxNote.length);
            note->setKeyIndex(dspxNote.keyNum);
            note->setCentShift(dspxNote.centShift);
            note->setLyric(QString::fromStdString(dspxNote.lyric));
            note->setLanguage(QString::fromStdString(dspxNote.language));
            note->setPronunciation(
                Pronunciation(QString::fromStdString(dspxNote.pronunciation.original), QString::fromStdString(dspxNote.pronunciation.edited)));
            QMap<QString, QJsonObject> workspace;
            for (const auto &[key, value] : dspxNote.workspace) {
                workspace[QString::fromStdString(key)] = JsonUtils::toQJsonValue(value).toObject();
            }
            note->setWorkspace(workspace);
            // note->setPhonemeInfo(Note::Original, decodePhonemes(dspxNote.phonemes.org));
            // note->setPhonemeInfo(Note::Edited, decodePhonemes(dspxNote.phonemes.edited));
            notes.append(note);
        }
        return notes;
    };
    auto decodeClips = [&](const std::vector<opendspx::ClipRef> &dspxClips, Track *track) {
        for (const auto &dspxClip : dspxClips) {
            if (dspxClip->type == opendspx::Clip::Type::Singing) {
                const auto castClip = std::static_pointer_cast<opendspx::SingingClip>(dspxClip);
                const auto clip = new SingingClip;
                clip->setName(QString::fromStdString(castClip->name));
                // TODO: language
                // clip->setDefaultLanguage(castClip->language);
                auto start = castClip->time.pos - castClip->time.clipStart;
                clip->setStart(start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                auto notes = decodeNotes(castClip->notes, start);
                for (const auto &note : notes)
                    clip->insertNote(note);
                clip->params =
                    std::move(decodeSingingParams(castClip->params, start, clip));
                QMap<QString, QJsonObject> workspace;
                for (const auto &[key, value] : castClip->workspace) {
                    workspace[QString::fromStdString(key)] = JsonUtils::toQJsonValue(value).toObject();
                }
                clip->workspace() = workspace;
                track->insertClip(clip);
            } else if (dspxClip->type == opendspx::Clip::Type::Audio) {
                const auto castClip = std::static_pointer_cast<opendspx::AudioClip>(dspxClip);
                const auto clip = new AudioClip;
                clip->setName(QString::fromStdString(castClip->name));
                auto start = castClip->time.pos - castClip->time.clipStart;
                clip->setStart(start);
                clip->setClipStart(castClip->time.clipStart);
                clip->setLength(castClip->time.length);
                clip->setClipLen(castClip->time.clipLen);
                clip->setGain(castClip->control.gain);
                clip->setMute(castClip->control.mute);
                clip->setPath(QString::fromStdString(castClip->path));
                QMap<QString, QJsonObject> workspace;
                for (const auto &[key, value] : castClip->workspace) {
                    workspace[QString::fromStdString(key)] = JsonUtils::toQJsonValue(value).toObject();
                }
                clip->workspace() = workspace;
                track->insertClip(clip);
            }
        }
    };

    auto decodeTracks = [&](const std::vector<opendspx::Track> &dspxTracks, AppModel *model_) {
        int i = 0;
        for (const auto &dspxTrack : dspxTracks) {
            const auto track = new Track;
            auto trackControl = TrackControl();
            trackControl.setGain(dspxTrack.control.gain);
            trackControl.setPan(dspxTrack.control.pan);
            trackControl.setMute(dspxTrack.control.mute);
            trackControl.setSolo(dspxTrack.control.solo);
            track->setName(QString::fromStdString(dspxTrack.name));
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

        opendspx::SerializationErrorList errors;
    };

    auto loadModel = [](const QString &filePath, opendspx::Model &outModel) -> LoadResult {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return LoadResult{LoadResult::Failure, {}};

        const QByteArray data = file.readAll();
        opendspx::SerializationErrorList errors;
        std::stringstream ss(data.toStdString(), std::ios::in);
        outModel = opendspx::Serializer::deserialize(ss, errors, opendspx::Serializer::CheckError);

        LoadResult result;
        if (errors.containsFatal()) {
            result.type = LoadResult::Failure;
        } else {
            result.type = LoadResult::Success;
        }
        result.errors = errors;
        return result;
    };

    opendspx::Model dspxModel;
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
        auto workspace = dspxModel.content.workspace;
        if (workspace.contains("loop")) {
            LoopSettings loopSettings;
            loopSettings.deserialize(JsonUtils::toQJsonValue(workspace["loop"]).toObject());
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

    auto encodeCurves = [&](const QList<Curve *> &dsCurves, std::vector<opendspx::ParamCurveRef> &curves) {
        for (const auto &dsCurve : dsCurves) {
            if (dsCurve->type() == Curve::CurveType::Draw) {
                const auto castCurve = dynamic_cast<DrawCurve *>(dsCurve);
                const auto curve = std::make_shared<opendspx::ParamCurveFree>();
                curve->start = castCurve->globalStart();
                curve->step = castCurve->step;
                for (const auto v : castCurve->values()) {
                    curve->values.push_back(v);
                }
                curves.push_back(curve);
            } else if (dsCurve->type() == Curve::CurveType::Anchor) {
                const auto castCurve = dynamic_cast<AnchorCurve *>(dsCurve);
                const auto curve = std::make_shared<opendspx::ParamCurveAnchor>();
                curve->start = dsCurve->globalStart();
                for (const auto dsNode : castCurve->nodes()) {
                    opendspx::AnchorNode node;
                    node.x = dsNode->pos();
                    node.y = dsNode->value();
                    if (dsNode->interpMode() == AnchorNode::None) {
                        node.interp = opendspx::AnchorNode::Interpolation::None;
                    } else if (dsNode->interpMode() == AnchorNode::Linear) {
                        node.interp = opendspx::AnchorNode::Interpolation::Linear;
                    } else if (dsNode->interpMode() == AnchorNode::Hermite) {
                        node.interp = opendspx::AnchorNode::Interpolation::Hermite;
                    } /*else if (dsNode->interpMode() == DsAnchorNode::Cubic) {
                        node.interp = opendspx::AnchorNode::Interpolation::Cubic;
                    }*/
                    curve->nodes.push_back(node);
                }
                curves.push_back(curve);
            }
        }
    };

    auto encodeSingingParam = [&](const Param &dsParam, opendspx::Param &param) {
        encodeCurves(dsParam.curves(Param::Original), param.original);
        encodeCurves(dsParam.curves(Param::Edited), param.edited);
        encodeCurves(dsParam.curves(Param::Envelope), param.transform);
    };

    auto encodeSingingParams = [&](const ParamInfo &dsParams, opendspx::Params &params) {
        encodeSingingParam(dsParams.pitch, params["pitch"]);
        encodeSingingParam(dsParams.expressiveness, params["expressiveness"]);
        encodeSingingParam(dsParams.energy, params["energy"]);
        encodeSingingParam(dsParams.breathiness, params["breathiness"]);
        encodeSingingParam(dsParams.voicing, params["voicing"]);
        encodeSingingParam(dsParams.tension, params["tension"]);
        encodeSingingParam(dsParams.gender, params["gender"]);
        encodeSingingParam(dsParams.velocity, params["velocity"]);
    };

    // auto encodePhonemes = [&](const QList<Phoneme> &dsPhonemes, QList<opendspx::Phoneme> &phonemes)
    // {
    //     for (const auto &dsPhoneme : dsPhonemes) {
    //         opendspx::Phoneme phoneme;
    //         phoneme.start = dsPhoneme.start;
    //         phoneme.token = dsPhoneme.name;
    //         if (dsPhoneme.type == Phoneme::PhonemeType::Ahead) {
    //             phoneme.type = opendspx::Phoneme::Type::Ahead;
    //         } else if (dsPhoneme.type == Phoneme::PhonemeType::Final) {
    //             phoneme.type = opendspx::Phoneme::Type::Final;
    //         } else {
    //             phoneme.type = opendspx::Phoneme::Type::Normal;
    //         }
    //         phonemes.append(phoneme);
    //     }
    // };

    auto encodeNotes = [&](const OverlappableSerialList<Note> &dsNotes, std::vector<opendspx::Note> &notes) {
        for (const auto dsNote : dsNotes) {
            opendspx::Note note;
            note.pos = dsNote->globalStart();
            note.length = dsNote->length();
            note.keyNum = dsNote->keyIndex();
            note.centShift = dsNote->centShift();
            note.lyric = dsNote->lyric().toStdString();
            note.language = dsNote->language().toStdString();
            note.pronunciation.original = dsNote->pronunciation().original.toStdString();
            note.pronunciation.edited = dsNote->pronunciation().edited.toStdString();
            // encodePhonemes(dsNote->phonemeInfo().original, note.phonemes.org);
            // encodePhonemes(dsNote->phonemeInfo().edited, note.phonemes.edited);
            for (const auto &[key, value] : dsNote->workspace().asKeyValueRange()) {
                note.workspace[key.toStdString()] = JsonUtils::fromQJsonValue(value);
            }
            notes.push_back(note);
        }
    };

    auto encodeClips = [&](const Track *dsTrack, opendspx::Track &track) {
        for (const auto clip : dsTrack->clips()) {
            if (clip->clipType() == Clip::Singing) {
                const auto singingClip = dynamic_cast<SingingClip *>(clip);
                auto singClip = std::make_shared<opendspx::SingingClip>();
                singClip->name = clip->name().toStdString();
                // TODO: language
                // singClip->language = singingClip->defaultLanguage();
                singClip->time.pos = clip->start() + clip->clipStart();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->control.gain = clip->gain();
                singClip->control.mute = clip->mute();
                for (const auto &[key, value] : clip->workspace().asKeyValueRange()) {
                    singClip->workspace[key.toStdString()] = JsonUtils::fromQJsonValue(value);
                }
                encodeNotes(singingClip->notes(), singClip->notes);
                encodeSingingParams(singingClip->params, singClip->params);
                track.clips.push_back(singClip);
            } else if (clip->clipType() == Clip::Audio) {
                const auto audioClip = dynamic_cast<AudioClip *>(clip);
                auto audioClipRef = std::make_shared<opendspx::AudioClip>();
                audioClipRef->name = clip->name().toStdString();
                audioClipRef->time.pos = clip->start() + clip->clipStart();
                audioClipRef->time.clipStart = clip->clipStart();
                audioClipRef->time.length = clip->length();
                audioClipRef->time.clipLen = clip->clipLen();
                audioClipRef->control.gain = clip->gain();
                audioClipRef->control.mute = clip->mute();
                audioClipRef->path = audioClip->path().toStdString();
                for (const auto &[key, value] : clip->workspace().asKeyValueRange()) {
                    audioClipRef->workspace[key.toStdString()] = JsonUtils::fromQJsonValue(value);
                }
                track.clips.push_back(audioClipRef);
            }
        }
    };

    auto encodeTracks = [&](const AppModel *model_, opendspx::Model &dspx) {
        for (const auto dsTrack : model_->tracks()) {
            opendspx::Track track;
            track.name = dsTrack->name().toStdString();
            // TODO: language
            // track.language = dsTrack->defaultLanguage();
            // track.g2pId = dsTrack->defaultG2pId();
            track.control.gain = dsTrack->control().gain();
            track.control.pan = dsTrack->control().pan();
            track.control.mute = dsTrack->control().mute();
            track.control.solo = dsTrack->control().solo();
            encodeClips(dsTrack, track);
            dspx.content.tracks.push_back(track);
        }
    };

    opendspx::Model dspxModel;
    dspxModel.content.global.centShift = 0; // TODO: where should I use centShift in the editor?
    auto &timeline = dspxModel.content.timeline;
    timeline.tempos.push_back(opendspx::Tempo(0, model->tempo()));
    timeline.timeSignatures.push_back(opendspx::TimeSignature(0, model->timeSignature().numerator,
                                                        model->timeSignature().denominator));

    encodeTracks(model, dspxModel);

    const auto loopSettings = appStatus->loopSettings.get();
    dspxModel.content.workspace["loop"] = JsonUtils::fromQJsonValue(loopSettings.serialize());

    auto saveModelToFile = [](const opendspx::Model &model_, const QString &filePath,
                              QString &msg) -> bool {
        opendspx::SerializationErrorList errors;
        std::stringstream ss(std::ios::out);
        opendspx::Serializer::serialize(ss, model_, errors, opendspx::Serializer::FailFast | opendspx::Serializer::CheckError);

        if (!errors.empty()) {
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

        auto jsonData = QByteArray::fromStdString(ss.str());

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
        errMsg = errorMsg;
        return false;
    }
    return true;
}