#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectConverters/MidiTextCodecConverter.h>

#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <opendspx/track.h>
#include <opendspx/timeline.h>
#include <opendspx/model.h>

#include <opendspxconverter/midi/midiconverter.h>
#include <opendspxconverter/midi/midiintermediatedata.h>

#include <QFile>
#include <QCoreApplication>

#include <algorithm>
#include <sstream>

static QList<Note *> convertNotes(const std::vector<opendspx::Note> &arrNotes, const int offset,
                                  const QString &language, const QString &defaultLyric) {
    QList<Note *> notes;
    for (const opendspx::Note &dsNote : arrNotes) {
        const auto note = new Note;
        note->setLocalStart(dsNote.pos - offset);
        note->setLength(dsNote.length);
        note->setKeyIndex(dsNote.keyNum);
        note->setLyric(dsNote.lyric.empty() ? defaultLyric : QString::fromStdString(dsNote.lyric));
        note->setLanguage(language);
        notes.push_back(note);
    }
    return notes;
}

static void convertClips(const opendspx::Track &track, Track *dsTrack, const QString &language,
                         const Timeline &timeline, const QString &defaultLyric) {
    for (auto &clip : track.clips) {
        if (clip->type == opendspx::Clip::Type::Singing) {
            const auto singClip = std::static_pointer_cast<opendspx::SingingClip>(clip);
            const auto singingClip = new SingingClip;
            singingClip->setName(QString::fromStdString(clip->name));
            const auto start = clip->time.pos - clip->time.clipStart;
            singingClip->setStart(start);
            singingClip->setClipStart(clip->time.clipStart);
            singingClip->setLength(clip->time.clipLen);
            singingClip->setClipLen(clip->time.clipLen + 960);
            singingClip->setDefaultLanguage(language);

            auto notes = convertNotes(singClip->notes, start, language, defaultLyric);
            for (const auto note : notes) {
                singingClip->insertNote(note);
            }
            dsTrack->insertClip(singingClip);
        } else if (clip->type == opendspx::Clip::Type::Audio) {
            const auto audioClip = new AudioClip;
            audioClip->setName(QString::fromStdString(clip->name));
            const auto start = clip->time.pos - clip->time.clipStart;
            audioClip->setStart(start);
            audioClip->setClipStart(clip->time.clipStart);
            audioClip->setLength(qMax(clip->time.length, clip->time.clipLen));
            audioClip->setClipLen(clip->time.clipLen);
            audioClip->setPath(
                QString::fromStdString(std::static_pointer_cast<opendspx::AudioClip>(clip)->path));
            // Ticks in the source file are authoritative; establish the
            // realtime anchor under the timeline that was just applied
            audioClip->syncTruthFromTicks(timeline);
            dsTrack->insertClip(audioClip);
        }
    }
}

static Track *convertTrack(const opendspx::Track &track, const QString &language,
                           const Timeline &timeline, const QString &defaultLyric) {
    const auto dsTrack = new Track;
    dsTrack->setName(QString::fromStdString(track.name));
    dsTrack->setDefaultLanguage(language);
    convertClips(track, dsTrack, language, timeline, defaultLyric);
    return dsTrack;
}

static std::vector<opendspx::Note> encodeNotes(const OverlappableSerialList<Note> &notes) {
    std::vector<opendspx::Note> arrNotes;
    for (const auto &note : notes) {
        opendspx::Note dsNote;
        dsNote.pos = note->globalStart();
        dsNote.length = note->length();
        dsNote.keyNum = note->keyIndex();
        dsNote.lyric = note->lyric().toStdString();
        arrNotes.push_back(dsNote);
    }
    return arrNotes;
}

static void encodeClips(const Track *dsTrack, opendspx::Track *track) {
    for (const auto &clip : dsTrack->clips()) {
        if (clip->clipType() == Clip::Singing) {
            const auto singingClip = dynamic_cast<SingingClip *>(clip);
            auto singClip = std::make_shared<opendspx::SingingClip>();
            singClip->name = clip->name().toStdString();
            singClip->time = {clip->start(), clip->clipLen(), clip->clipStart(), clip->clipLen()};
            singClip->notes = encodeNotes(singingClip->notes());
            track->clips.push_back(singClip);
        } else if (clip->clipType() == Clip::Audio) {
            const auto audioClip = dynamic_cast<AudioClip *>(clip);
            auto audioClipRef = std::make_shared<opendspx::AudioClip>();
            audioClipRef->name = clip->name().toStdString();
            audioClipRef->time = {clip->start(), clip->clipLen(), clip->clipStart(),
                                  clip->clipLen()};
            audioClipRef->path = audioClip->path().toStdString();
            track->clips.push_back(audioClipRef);
        }
    }
}

static void encodeTracks(const AppModel *model, opendspx::Model &dspx) {
    for (const auto &dsTrack : model->tracks()) {
        opendspx::Track track;
        track.name = dsTrack->name().toStdString();
        encodeClips(dsTrack, &track);
        dspx.content.tracks.push_back(track);
    }
}

MidiConverter::MidiConverter() {
}

static QString toneNumToToneName(const int num) {
    static const QString tones[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B"};

    int step = num % 12;
    int octave = num / 12 - 1;

    if (num < 0) {
        octave -= 1;
        step = (step + 12) % 12;
    }

    if (octave < -1 || step < 0 || step >= 12)
        return QCoreApplication::translate("MidiConverter", "Invalid tone or octave");

    // Scientific pitch names are stable identifiers, not localized quantities.
    return tones[step] + QString::number(octave);
}

QList<MidiImportTrackInfo>
    buildMidiTrackInfoList(const std::vector<opendspx::MidiIntermediateData::Track> &tracks) {
    QList<MidiImportTrackInfo> result;
    result.reserve(static_cast<int>(tracks.size()));

    for (const auto &track : tracks) {
        MidiImportTrackInfo info;
        info.name = QByteArray::fromStdString(track.title);
        if (!track.notes.empty()) {
            const auto minMaxNotes =
                std::minmax_element(track.notes.begin(), track.notes.end(),
                                    [](const auto &a, const auto &b) { return a.key < b.key; });
            info.rangeText =
                QStringLiteral("%1 - %2").arg(toneNumToToneName(minMaxNotes.first->key),
                                              toneNumToToneName(minMaxNotes.second->key));
        }
        info.noteCount = static_cast<int>(track.notes.size());
        info.selectedByDefault = !track.notes.empty();
        for (const auto &note : track.notes) {
            info.lyrics.append(QByteArray::fromStdString(note.lyric));
        }
        result.append(info);
    }
    return result;
}

bool MidiConverter::load(const QString &path, AppModel *model, QString &errMsg,
                         const ImportMode mode) {
    LoadOptions options;
    return loadInteractive(path, model, errMsg, mode, options) == LoadStatus::Success;
}

MidiConverter::LoadStatus MidiConverter::loadInteractive(const QString &path, AppModel *model,
                                                         QString &errMsg, const ImportMode mode,
                                                         LoadOptions &options) {
    auto parseData = MidiFileParser::parse(path);
    if (!parseData.valid) {
        errMsg = parseData.errorMessage;
        return LoadStatus::Failed;
    }

    // Re-derives the track layout when the interactive UI toggles
    // separate-channels; keeps midiMediate in sync so the final selection
    // indexes into the reconverted tracks.
    struct Reconverter final : MidiTrackReconverter {
        MidiParseData &data;

        explicit Reconverter(MidiParseData &data) : data(data) {
        }

        QList<MidiImportTrackInfo> reconvert(bool separateChannels) override {
            opendspx::MidiConverter converter;
            opendspx::MidiConverter::Error error;
            std::stringstream ss(data.rawData.toStdString(), std::ios::in);
            auto updated = converter.convertMidiToIntermediate(ss, error, {separateChannels});
            if (error == opendspx::MidiConverter::Error::NoError)
                data.mediate = std::move(updated);
            return buildMidiTrackInfoList(data.mediate.tracks());
        }
    } reconverter{parseData};

    MidiImportOptions choice;
    if (!chooseImportOptions(parseData.trackInfos, reconverter, mode == NewProject,
                             mode == NewProject, choice)) {
        return LoadStatus::Canceled;
    }

    const auto language = importLanguage();
    auto generated = MidiTrackGenerator::generateTracks(parseData, choice, language,
                                                        defaultLyric(language), model->timeline());
    if (!generated.errorMessage.isEmpty()) {
        errMsg = generated.errorMessage;
        return LoadStatus::Failed;
    }

    // Apply the imported timeline first so generated audio clips anchor their
    // realtime truth under the final tempo map.
    if (generated.hasTimeline) {
        auto newTimeline = model->timeline();
        if (!generated.timeSignatures.isEmpty())
            newTimeline.setTimeSignatures(generated.timeSignatures);
        if (!generated.tempos.isEmpty())
            newTimeline.setTempos(generated.tempos);
        if (newTimeline != model->timeline())
            model->setTimeline(std::move(newTimeline));
    }

    if (generated.tracks.isEmpty()) {
        errMsg = QCoreApplication::translate("MidiConverter",
                                             "No MIDI tracks were selected for import.");
        return LoadStatus::Failed;
    }
    int count = 0;
    for (const auto track : generated.tracks) {
        model->insertTrack(track, count);
        count++;
    }
    options.importTempo = choice.importTempo && !generated.tempos.isEmpty();
    options.importTimeSignature = choice.importTimeSignature && !generated.timeSignatures.isEmpty();
    return LoadStatus::Success;
}

MidiParseData MidiFileParser::parse(const QString &path) {
    MidiParseData result;
    result.path = path;

    QFile midiFile(path);
    if (!midiFile.open(QIODevice::ReadOnly)) {
        result.errorMessage =
            QCoreApplication::translate("MidiConverter", "Failed to read MIDI file.\npath: %1")
                .arg(path);
        return result;
    }

    result.rawData = midiFile.readAll();
    opendspx::MidiConverter converter;
    opendspx::MidiConverter::Error midiError;
    std::stringstream ss(result.rawData.toStdString(), std::ios::in);
    result.mediate = converter.convertMidiToIntermediate(ss, midiError, {true});
    if (midiError != opendspx::MidiConverter::Error::NoError) {
        result.errorMessage = QCoreApplication::translate(
                                  "MidiConverter", "Failed to load MIDI file.\npath: %1\ntype: %L2")
                                  .arg(path)
                                  .arg(static_cast<int>(midiError));
        return result;
    }

    result.trackInfos = buildMidiTrackInfoList(result.mediate.tracks());
    result.valid = true;
    return result;
}

MidiGenerationResult MidiTrackGenerator::generateTracks(MidiParseData &data,
                                                        const MidiImportOptions &choice,
                                                        const QString &language,
                                                        const QString &defaultLyric,
                                                        const Timeline &timeline) {
    MidiGenerationResult result;
    const auto codec = choice.codec;
    const auto selectTrackIds = choice.selectedTrackIndices;

    std::vector<opendspx::MidiIntermediateData::Track> selectedTracks;
    selectedTracks.reserve(selectTrackIds.size());
    for (const auto index : selectTrackIds) {
        if (index < 0 || static_cast<qsizetype>(index) >= data.mediate.tracks().size()) {
            result.errorMessage =
                QCoreApplication::translate(
                    "MidiConverter", "Invalid MIDI track selection while importing.\npath: %1")
                    .arg(data.path);
            return result;
        }
        selectedTracks.push_back(data.mediate.tracks().at(index));
    }

    auto decodeText = [&](const std::string &value) -> std::string {
        if (value.empty())
            return {};
        const auto decoded =
            MidiTextCodecConverter::decode(QByteArray::fromStdString(value), codec);
        if (decoded.isEmpty())
            return value;
        return decoded.toStdString();
    };

    for (auto &track : selectedTracks) {
        track.title = decodeText(track.title);
        for (auto &note : track.notes) {
            note.lyric = decodeText(note.lyric);
        }
    }

    data.mediate = {
        data.mediate.resolution(),
        choice.importTempo ? data.mediate.tempos()
                           : std::vector<opendspx::MidiIntermediateData::Tempo>{},
        choice.importTimeSignature ? data.mediate.timeSignatures()
                                   : std::vector<opendspx::MidiIntermediateData::TimeSignature>{},
        data.mediate.markers(),
        selectedTracks,
    };

    opendspx::MidiConverter converter;
    const auto midiDspx = converter.convertIntermediateToDspx(data.mediate);

    const auto &timelineModel = midiDspx.content.timeline;
    const auto hasTimeSignature = !timelineModel.timeSignatures.empty();
    const auto hasTempo = !timelineModel.tempos.empty();

    if (hasTimeSignature) {
        for (const auto &ts : timelineModel.timeSignatures) {
            if (ts.denominator != 2 && ts.denominator != 4 && ts.denominator != 8 &&
                ts.denominator != 16) {
                result.errorMessage =
                    QCoreApplication::translate(
                        "MidiConverter",
                        "Failed to load MIDI file.\ntimeSignatures denominator must be: "
                        "%L1, %L2, %L3, %L4\ncurrent denominator: %L5")
                        .arg(2)
                        .arg(4)
                        .arg(8)
                        .arg(16)
                        .arg(ts.denominator);
                return result;
            }
        }
    }

    if (hasTimeSignature) {
        result.timeSignatures.reserve(static_cast<qsizetype>(timelineModel.timeSignatures.size()));
        for (const auto &ts : timelineModel.timeSignatures)
            result.timeSignatures.append(TimeSignature(ts.index, ts.numerator, ts.denominator));
    }
    if (hasTempo) {
        result.tempos.reserve(static_cast<qsizetype>(timelineModel.tempos.size()));
        for (const auto &tempo : timelineModel.tempos)
            result.tempos.append({tempo.pos, tempo.value});
    }
    result.hasTimeline = hasTimeSignature || hasTempo;

    result.tracks.reserve(static_cast<qsizetype>(midiDspx.content.tracks.size()));
    for (const auto &track : midiDspx.content.tracks)
        result.tracks.append(convertTrack(track, language, timeline, defaultLyric));

    return result;
}

bool MidiConverter::save(const QString &path, AppModel *model, QString &errMsg) {
    opendspx::Model dspx;
    opendspx::MidiConverter midiConverter;

    for (const auto &tempo : model->timeline().tempos())
        dspx.content.timeline.tempos.push_back({tempo.pos, tempo.value});
    for (const auto &ts : model->timeline().timeSignatures())
        dspx.content.timeline.timeSignatures.push_back({ts.barIndex, ts.numerator, ts.denominator});

    encodeTracks(model, dspx);

    auto midiMediate = midiConverter.convertDspxToIntermediate(dspx);
    std::stringstream ss(std::ios::out);
    midiConverter.convertIntermediateToMidi(ss, midiMediate);

    auto saveMidiToFile = [](const QByteArray &midi, const QString &filePath,
                             QString &msg) -> bool {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            msg +=
                QCoreApplication::translate("MidiConverter", "Failed to open file for writing: %1")
                    .arg(filePath);
            return false;
        }

        const qint64 written = file.write(midi);
        file.close();

        if (written != midi.size()) {
            msg +=
                QCoreApplication::translate("MidiConverter", "Failed to write all data to file: %1")
                    .arg(filePath);
            return false;
        }

        return true;
    };

    QString msg;
    const auto result = saveMidiToFile(QByteArray::fromStdString(ss.str()), path, msg);
    if (!result) {
        errMsg = QCoreApplication::translate("MidiConverter",
                                             "Failed to save MIDI file.\npath: %1\nerror: %2")
                     .arg(path, msg);
        return false;
    }
    return true;
}
