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
        note->setLyric(dsNote.lyric.empty() ? defaultLyric
                                            : QString::fromStdString(dsNote.lyric));
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

static void convertTracks(const opendspx::Model &dspx, AppModel *model, const QString &language,
                          const QString &defaultLyric) {
    int count = 0;
    for (const auto &track : dspx.content.tracks) {
        const auto dsTrack = new Track;
        dsTrack->setName(QString::fromStdString(track.name));
        dsTrack->setDefaultLanguage(language);
        convertClips(track, dsTrack, language, model->timeline(), defaultLyric);
        model->insertTrack(dsTrack, count);
        count++;
    }
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

static QList<MidiImportTrackInfo>
    buildTrackInfoList(const std::vector<opendspx::MidiIntermediateData::Track> &tracks) {
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
    const auto midiConverter = std::make_unique<opendspx::MidiConverter>();
    const QString language = importLanguage();

    QFile midiFile(path);
    if (!midiFile.open(QIODevice::ReadOnly)) {
        errMsg = QCoreApplication::translate("MidiConverter", "Failed to read MIDI file.\npath: %1")
                     .arg(path);
        return LoadStatus::Failed;
    }

    const QByteArray midiData = midiFile.readAll();
    opendspx::MidiConverter::Error midiError;
    std::stringstream ss(midiData.toStdString(), std::ios::in);
    auto midiMediate = midiConverter->convertMidiToIntermediate(ss, midiError, {true});
    if (midiError != opendspx::MidiConverter::Error::NoError) {
        errMsg = QCoreApplication::translate("MidiConverter",
                                             "Failed to load MIDI file.\npath: %1\ntype: %L2")
                     .arg(path)
                     .arg(static_cast<int>(midiError));
        return LoadStatus::Failed;
    }

    // Re-derives the track layout when the interactive UI toggles
    // separate-channels; keeps midiMediate in sync so the final selection below
    // indexes into the reconverted tracks.
    struct Reconverter final : MidiTrackReconverter {
        opendspx::MidiConverter &converter;
        const QByteArray &midiData;
        opendspx::MidiIntermediateData &midiMediate;
        opendspx::MidiConverter::Error &midiError;

        Reconverter(opendspx::MidiConverter &converter, const QByteArray &midiData,
                    opendspx::MidiIntermediateData &midiMediate,
                    opendspx::MidiConverter::Error &midiError)
            : converter(converter), midiData(midiData), midiMediate(midiMediate),
              midiError(midiError) {
        }

        QList<MidiImportTrackInfo> reconvert(bool separateChannels) override {
            std::stringstream ss(midiData.toStdString(), std::ios::in);
            auto updated = converter.convertMidiToIntermediate(ss, midiError, {separateChannels});
            if (midiError == opendspx::MidiConverter::Error::NoError)
                midiMediate = std::move(updated);
            return buildTrackInfoList(midiMediate.tracks());
        }
    } reconverter{*midiConverter, midiData, midiMediate, midiError};

    MidiImportOptions choice;
    if (!chooseImportOptions(buildTrackInfoList(midiMediate.tracks()), reconverter,
                             mode == NewProject, mode == NewProject, choice)) {
        return LoadStatus::Canceled;
    }

    const auto codec = choice.codec;
    const auto selectTrackIds = choice.selectedTrackIndices;

    std::vector<opendspx::MidiIntermediateData::Track> selectedTracks;
    selectedTracks.reserve(selectTrackIds.size());
    for (const auto index : selectTrackIds) {
        selectedTracks.push_back(midiMediate.tracks().at(index));
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

    midiMediate = {
        midiMediate.resolution(),
        choice.importTempo ? midiMediate.tempos()
                          : std::vector<opendspx::MidiIntermediateData::Tempo>{},
        choice.importTimeSignature ? midiMediate.timeSignatures()
                                  : std::vector<opendspx::MidiIntermediateData::TimeSignature>{},
        midiMediate.markers(),
        selectedTracks,
    };

    const auto midiDspx = midiConverter->convertIntermediateToDspx(midiMediate);

    const auto &timeline = midiDspx.content.timeline;
    const auto hasTimeSignature = !timeline.timeSignatures.empty();
    const auto hasTempo = !timeline.tempos.empty();

    if (hasTimeSignature) {
        for (const auto &ts : timeline.timeSignatures) {
            if (ts.denominator != 2 && ts.denominator != 4 && ts.denominator != 8 &&
                ts.denominator != 16) {
                errMsg = QCoreApplication::translate(
                             "MidiConverter",
                             "Failed to load MIDI file.\ntimeSignatures denominator must be: "
                             "%L1, %L2, %L3, %L4\ncurrent denominator: %L5")
                             .arg(2)
                             .arg(4)
                             .arg(8)
                             .arg(16)
                             .arg(ts.denominator);
                return LoadStatus::Failed;
            }
        }
    }

    if (mode != NewProject && mode != AppendToProject) {
        errMsg = QCoreApplication::translate("MidiConverter", "Unsupported MIDI import mode.");
        return LoadStatus::Failed;
    }

    auto newTimeline = model->timeline();
    if (hasTimeSignature) {
        QList<TimeSignature> signatures;
        signatures.reserve(static_cast<qsizetype>(timeline.timeSignatures.size()));
        for (const auto &ts : timeline.timeSignatures)
            signatures.append(TimeSignature(ts.index, ts.numerator, ts.denominator));
        newTimeline.setTimeSignatures(std::move(signatures));
    }
    if (hasTempo) {
        QList<Tempo> tempos;
        tempos.reserve(static_cast<qsizetype>(timeline.tempos.size()));
        for (const auto &tempo : timeline.tempos)
            tempos.append({tempo.pos, tempo.value});
        newTimeline.setTempos(std::move(tempos));
    }
    if (newTimeline != model->timeline())
        model->setTimeline(std::move(newTimeline));

    if (!midiDspx.content.tracks.empty()) {
        convertTracks(midiDspx, model, language, defaultLyric(language));
        options.importTempo = choice.importTempo && hasTempo;
        options.importTimeSignature = choice.importTimeSignature && hasTimeSignature;
        return LoadStatus::Success;
    }
    errMsg =
        QCoreApplication::translate("MidiConverter", "No MIDI tracks were selected for import.");
    return LoadStatus::Failed;
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
