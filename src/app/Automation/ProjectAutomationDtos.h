#ifndef PROJECTAUTOMATIONDTOS_H
#define PROJECTAUTOMATIONDTOS_H

#include "AutomationTypes.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include <lite/ProjectModel/AppModel/Params.h>
#include <lite/ProjectModel/AppModel/Phonemes.h>
#include <lite/ProjectModel/AppModel/ProjectModelData.h>
#include <lite/ProjectModel/AppModel/Pronunciation.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>

#include <QMap>

#include <memory>
#include <optional>

class Clip;
class Note;
class SingingClip;
class Timeline;
class Track;

namespace Automation {

    struct TrackPropertiesDto {
        TrackId id;
        QString name;
        double gain = 0.0;
        double pan = 0.0;
        bool mute = false;
        bool solo = false;
    };

    struct ClipPropertiesDto {
        ClipId id;
        QString name;
        int start = 0;
        int length = 0;
        int clipStart = 0;
        int clipLen = 0;
        double gain = 0.0;
        bool mute = false;
        double trimStartMs = -1.0;
        double playLengthMs = -1.0;
        double materialLengthMs = -1.0;
    };

    struct NoteDraftDto {
        QString clientRef;
        int localStart = 0;
        int length = 0;
        int keyIndex = 60;
        int centShift = 0;
        QString lyric;
        QString language;
        Pronunciation pronunciation;
        QStringList pronunciationCandidates;
        Phonemes phonemes;
        bool lineFeed = false;
        QMap<QString, QJsonObject> workspace;
    };

    struct AnchorNodeDraftDto {
        int position = 0;
        int value = 0;
        AnchorNode::InterpMode interpolation = AnchorNode::Hermite;
    };

    struct CurveDraftDto {
        enum class Type {
            Draw,
            Anchor,
        };

        Type type = Type::Draw;
        int localStart = 0;
        int step = 5;
        QList<int> values;
        QList<AnchorNodeDraftDto> nodes;
    };

    struct ParamCurvesDraftDto {
        ParamInfo::Name name = ParamInfo::Unknown;
        Param::Type type = Param::Unknown;
        QList<CurveDraftDto> curves;
    };

    struct ClipDraftDto {
        enum class Type {
            Singing,
            Audio,
        };

        QString clientRef;
        Type type = Type::Singing;
        ClipPropertiesDto properties;
        QMap<QString, QJsonObject> workspace;

        QString defaultLanguage;
        QList<NoteDraftDto> notes;
        QList<ParamCurvesDraftDto> params;
        bool usesTrackVoiceContext = true;
        SingerInfo ownSingerInfo;
        SpeakerInfo ownSpeakerInfo;
        SpeakerMixModel::SpeakerMixData ownSpeakerMixData;

        QString audioPath;
        AudioPathInfo audioPathInfo;
        AudioClip::PathStatus audioPathStatus = AudioClip::PathStatus::Normal;
        AudioInfoModel audioInfo;
        bool hasRealTimeAnchor = false;
    };

    struct AudioAssetSnapshotDto {
        QString path;
        AudioPathInfo pathInfo;
        QJsonObject formatData;
        quint64 sourceGeneration = 0;

        friend bool operator==(const AudioAssetSnapshotDto &left,
                               const AudioAssetSnapshotDto &right) {
            return left.path == right.path &&
                   left.pathInfo.relativeDir == right.pathInfo.relativeDir &&
                   left.pathInfo.sha512 == right.pathInfo.sha512 &&
                   left.formatData == right.formatData &&
                   left.sourceGeneration == right.sourceGeneration;
        }
    };

    struct TrackDraftDto {
        QString clientRef;
        QString name;
        int colorIndex = 0;
        double gain = 0.0;
        double pan = 0.0;
        bool mute = false;
        bool solo = false;
        QString defaultLanguage;
        SingerInfo singerInfo;
        SpeakerInfo speakerInfo;
        SpeakerMixModel::SpeakerMixData speakerMixData;
        QList<ClipDraftDto> clips;
    };

    struct ClipInsertDto {
        TrackId trackId;
        ClipDraftDto clip;
    };

    struct DocumentDraftDto {
        Timeline timeline;
        TrackControl masterControl;
        QList<TrackDraftDto> tracks;
        LoopSettings loopSettings;
    };

    struct BatchImportItemDraftDto {
        std::optional<TrackId> existingTrackId;
        TrackDraftDto newTrack;
        QList<ClipDraftDto> clips;
    };

    struct BatchImportDraftDto {
        Timeline timeline;
        QList<BatchImportItemDraftDto> items;
    };

    [[nodiscard]] TrackPropertiesDto trackPropertiesDto(const Track &track);
    [[nodiscard]] ClipPropertiesDto clipPropertiesDto(const Clip &clip);
    [[nodiscard]] AudioAssetSnapshotDto audioAssetSnapshotDto(const AudioClip &clip);
    [[nodiscard]] NoteDraftDto noteDraftDto(const Note &note);
    [[nodiscard]] CurveDraftDto curveDraftDto(const Curve &curve);
    [[nodiscard]] ClipDraftDto clipDraftDto(const Clip &clip);
    [[nodiscard]] TrackDraftDto trackDraftDto(const Track &track);
    [[nodiscard]] DocumentDraftDto documentDraftDto(const ProjectModelData &data,
                                                    const LoopSettings &loopSettings = {});

    [[nodiscard]] std::unique_ptr<Note>
        buildNote(const NoteDraftDto &draft, SingingClip *clip,
                  QList<CreatedObjectRef> *createdObjects = nullptr);
    [[nodiscard]] std::unique_ptr<Curve> buildCurve(const CurveDraftDto &draft);
    [[nodiscard]] std::unique_ptr<Clip>
        buildClip(const ClipDraftDto &draft, const Track *targetTrack, const Timeline &timeline,
                  QList<CreatedObjectRef> *createdObjects = nullptr);
    [[nodiscard]] std::unique_ptr<Track>
        buildTrack(const TrackDraftDto &draft, const Timeline &timeline,
                   QList<CreatedObjectRef> *createdObjects = nullptr);
    [[nodiscard]] ProjectModelData
        buildProjectModelData(const DocumentDraftDto &draft,
                              QList<CreatedObjectRef> *createdObjects = nullptr);

    [[nodiscard]] QByteArray fingerprint(const TrackPropertiesDto &properties);
    [[nodiscard]] QByteArray fingerprint(const ClipPropertiesDto &properties);
    [[nodiscard]] QByteArray fingerprint(const SingerInfo &singerInfo);
    [[nodiscard]] QByteArray fingerprint(const SpeakerInfo &speakerInfo);
    [[nodiscard]] QByteArray fingerprint(const SpeakerMixModel::SpeakerMixData &speakerMixData);
    [[nodiscard]] QByteArray fingerprint(const TrackDraftDto &draft);
    [[nodiscard]] QByteArray fingerprint(const QList<ClipInsertDto> &clips);
    [[nodiscard]] QByteArray fingerprint(const DocumentDraftDto &draft);
    [[nodiscard]] QByteArray fingerprint(const BatchImportDraftDto &draft);

    [[nodiscard]] AutomationResult<AutomationUnit> validate(const ClipDraftDto &draft);
    [[nodiscard]] AutomationResult<AutomationUnit> validate(const TrackDraftDto &draft);
    [[nodiscard]] AutomationResult<AutomationUnit> validate(const DocumentDraftDto &draft);
    [[nodiscard]] AutomationResult<AutomationUnit>
        validateClientRefs(const QList<NoteDraftDto> &notes);
    [[nodiscard]] AutomationResult<AutomationUnit>
        validateClientRefs(const QList<ClipInsertDto> &clips);
    [[nodiscard]] AutomationResult<AutomationUnit>
        validateClientRefs(const BatchImportDraftDto &batch);

} // namespace Automation

#endif // PROJECTAUTOMATIONDTOS_H
