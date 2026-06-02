//
// Created by fluty on 24-2-18.
//

#include "ClipsInfo.h"

#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Modules/PackageManager/PackageManager.h"

namespace {

QJsonObject serializeSingerInfo(const SingerInfo &singerInfo) {
    if (singerInfo.isEmpty())
        return {};

    QJsonObject obj;
    QJsonObject identifierObj;
    const auto identifier = singerInfo.identifier();
    if (!identifier.singerId.isEmpty())
        identifierObj["singerId"] = identifier.singerId;
    if (!identifier.packageId.isEmpty())
        identifierObj["packageId"] = identifier.packageId;
    if (!identifier.packageVersion.isNull())
        identifierObj["packageVersion"] = identifier.packageVersion.toString();
    if (!identifierObj.isEmpty())
        obj["identifier"] = identifierObj;
    if (!singerInfo.name().isEmpty())
        obj["name"] = singerInfo.name();
    if (!singerInfo.defaultLanguage().isEmpty())
        obj["defaultLanguage"] = singerInfo.defaultLanguage();
    return obj;
}

SingerInfo deserializeSingerInfo(const QJsonObject &obj) {
    if (obj.isEmpty())
        return {};

    const auto identifierObj = obj["identifier"].toObject();
    SingerIdentifier identifier;
    identifier.singerId = identifierObj["singerId"].toString();
    identifier.packageId = identifierObj["packageId"].toString();
    identifier.packageVersion = QVersionNumber::fromString(identifierObj["packageVersion"].toString());
    if (identifier.isEmpty())
        return {};

    const auto resolved = packageManager->findSingerByIdentifier(identifier);
    if (!resolved.isEmpty())
        return resolved;

    return SingerInfo(identifier, obj["name"].toString(), {}, {}, obj["defaultLanguage"].toString(), {});
}

QJsonObject serializeSpeakerInfo(const SpeakerInfo &speakerInfo) {
    if (speakerInfo.isEmpty())
        return {};

    QJsonObject obj;
    if (!speakerInfo.id().isEmpty())
        obj["id"] = speakerInfo.id();
    if (!speakerInfo.name().isEmpty())
        obj["name"] = speakerInfo.name();
    if (!speakerInfo.toneMin().isEmpty())
        obj["toneMin"] = speakerInfo.toneMin();
    if (!speakerInfo.toneMax().isEmpty())
        obj["toneMax"] = speakerInfo.toneMax();
    return obj;
}

SpeakerInfo deserializeSpeakerInfo(const QJsonObject &obj) {
    if (obj.isEmpty())
        return {};

    const auto id = obj["id"].toString();
    if (id.isEmpty())
        return {};

    return SpeakerInfo(id, obj["name"].toString(), obj["toneMin"].toString(),
                       obj["toneMax"].toString());
}

SpeakerInfo resolveSpeakerInfo(const SingerInfo &singerInfo, const SpeakerInfo &fallback) {
    if (fallback.isEmpty())
        return {};

    for (const auto &speaker : singerInfo.speakers()) {
        if (speaker.id() == fallback.id())
            return speaker;
    }
    return fallback;
}

}

static QJsonObject serializeClipCommon(const Clip *clip) {
    QJsonObject obj;
    obj["name"] = clip->name();
    obj["start"] = clip->start();
    obj["length"] = clip->length();
    obj["clipStart"] = clip->clipStart();
    obj["clipLen"] = clip->clipLen();
    obj["gain"] = clip->gain();
    obj["mute"] = clip->mute();

    QJsonObject ws;
    const auto workspace = clip->workspace();
    for (auto it = workspace.begin(); it != workspace.end(); ++it)
        ws[it.key()] = it.value();
    obj["workspace"] = ws;

    return obj;
}

static void deserializeClipCommon(Clip *clip, const QJsonObject &obj) {
    clip->setName(obj["name"].toString());
    clip->setStart(obj["start"].toInt());
    clip->setLength(obj["length"].toInt());
    clip->setClipStart(obj["clipStart"].toInt());
    clip->setClipLen(obj["clipLen"].toInt());
    clip->setGain(obj["gain"].toDouble());
    clip->setMute(obj["mute"].toBool());

    const auto ws = obj["workspace"].toObject();
    for (auto it = ws.begin(); it != ws.end(); ++it)
        clip->workspace()[it.key()] = it.value().toObject();
}

QJsonObject ClipsInfo::serializeToJson(const ClipsInfo &info) {
    QJsonArray clipList;
    for (const auto clip : info.clips) {
        QJsonObject obj = serializeClipCommon(clip);

        if (clip->clipType() == IClip::Singing) {
            const auto singingClip = static_cast<SingingClip *>(clip);
            obj["type"] = "singing";
            obj["defaultLanguage"] = singingClip->defaultLanguage();
            obj["useTrackSingerInfo"] = singingClip->useTrackSingerInfo.get();
            obj["useTrackSpeakerInfo"] = singingClip->useTrackSpeakerInfo.get();
            obj["singer"] = serializeSingerInfo(singingClip->singerInfo());
            obj["speaker"] = serializeSpeakerInfo(singingClip->speakerInfo());

            QJsonArray notesArr;
            const auto notes = singingClip->notes().toList();
            for (const auto note : notes)
                notesArr.append(note->serialize());
            obj["notes"] = notesArr;

        } else if (clip->clipType() == IClip::Audio) {
            const auto audioClip = static_cast<AudioClip *>(clip);
            obj["type"] = "audio";
            obj["path"] = audioClip->path();
        }

        clipList.append(obj);
    }

    QJsonArray offsetsArr;
    for (const auto offset : info.trackIndexOffsets)
        offsetsArr.append(offset);

    QJsonObject root;
    root["clips"] = clipList;
    root["trackIndexOffsets"] = offsetsArr;
    return root;
}

ClipsInfo ClipsInfo::deserializeFromJson(const QJsonObject &root) {
    ClipsInfo info;

    const auto clipList = root["clips"].toArray();
    for (const auto &val : clipList) {
        const auto obj = val.toObject();
        const auto type = obj["type"].toString();

        Clip *clip = nullptr;
        if (type == "singing") {
            auto singingClip = new SingingClip;
            singingClip->setDefaultLanguage(obj["defaultLanguage"].toString());

            const auto notesArr = obj["notes"].toArray();
            for (const auto &noteVal : notesArr) {
                auto note = new Note;
                note->deserialize(noteVal.toObject());
                singingClip->insertNote(note);
            }
            if (!obj["useTrackSingerInfo"].toBool(true) ||
                !obj["useTrackSpeakerInfo"].toBool(true)) {
                const auto singerInfo = deserializeSingerInfo(obj["singer"].toObject());
                const auto speakerInfo = resolveSpeakerInfo(
                    singerInfo, deserializeSpeakerInfo(obj["speaker"].toObject()));
                singingClip->setOwnSingerAndSpeaker(singerInfo, speakerInfo);
            }
            clip = singingClip;

        } else if (type == "audio") {
            auto audioClip = new AudioClip;
            audioClip->setPath(obj["path"].toString());
            clip = audioClip;
        }

        if (clip) {
            deserializeClipCommon(clip, obj);
            info.clips.append(clip);
        }
    }

    const auto offsetsArr = root["trackIndexOffsets"].toArray();
    for (const auto &val : offsetsArr)
        info.trackIndexOffsets.append(val.toInt());

    return info;
}
