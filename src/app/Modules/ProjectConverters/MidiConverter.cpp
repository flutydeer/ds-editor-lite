#include "MidiConverter.h"
#include "MidiConverterDialog.h"
#include "MidiTextCodecConverter.h"

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include "Model/AppModel/Track.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppOptions/AppOptions.h"

#include <opendspx/track.h>
#include <opendspx/timeline.h>
#include <opendspx/model.h>

#include <opendspxconverter/midi/midiconverter.h>
#include <opendspxconverter/midi/midiintermediatedata.h>

#include <QDialog>
#include <QFile>

#include <algorithm>
#include <sstream>

static int showImportDialog() {
    MessageDialog msgBox;
    msgBox.setWindowTitle("MIDI Import");
    msgBox.setMessage("Do you want to create a new track or use a new project?");
    msgBox.addButton("New Track", 1);
    msgBox.addButton("New Project", 2);
    msgBox.addButton("Cancel", 0);
    return msgBox.exec();
}

static void showErrorDialog(const QString &title, const QString &message) {
    MessageDialog msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setMessage(message);
    msgBox.addAccentButton("Ok", 1);
    msgBox.exec();
}

static QList<Note *> convertNotes(const std::vector<opendspx::Note> &arrNotes, const int offset,
                                  const QString &language) {
    QList<Note *> notes;
    for (const opendspx::Note &dsNote : arrNotes) {
        const auto note = new Note;
        note->setLocalStart(dsNote.pos - offset);
        note->setLength(dsNote.length);
        note->setKeyIndex(dsNote.keyNum);
        note->setLyric(dsNote.lyric.empty() ? appOptions->general()->defaultLyric : QString::fromStdString(dsNote.lyric));
        note->setLanguage(language);
        notes.push_back(note);
    }
    return notes;
}

static void convertClips(const opendspx::Track &track, Track *dsTrack, const QString &language) {
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

            auto notes = convertNotes(singClip->notes, start, language);
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
            audioClip->setLength(clip->time.clipLen);
            audioClip->setClipLen(clip->time.clipLen);
            audioClip->setPath(QString::fromStdString(std::static_pointer_cast<opendspx::AudioClip>(clip)->path));
            dsTrack->insertClip(audioClip);
        }
    }
}

static void convertTracks(const opendspx::Model &dspx, QList<int> selectTrackIds, AppModel *model,
                          const QString &language) {
    int count = 0;
    for (const auto &i : selectTrackIds) {
        const auto &track = dspx.content.tracks[i];
        const auto dsTrack = new Track;
        dsTrack->setName(QString::fromStdString(track.name));
        dsTrack->setDefaultLanguage(language);
        convertClips(track, dsTrack, language);
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
        return QStringLiteral("Invalid tone or octave");

    return tones[step] + QString::number(octave);
}

static QList<MidiConverterDialog::TrackInfo> buildTrackInfoList(
    const std::vector<opendspx::MidiIntermediateData::Track> &tracks) {
    QList<MidiConverterDialog::TrackInfo> result;
    result.reserve(static_cast<int>(tracks.size()));

    for (const auto &track : tracks) {
        MidiConverterDialog::TrackInfo info;
        info.name = QByteArray::fromStdString(track.title);
        if (!track.notes.empty()) {
            const auto minMaxNotes = std::minmax_element(
                track.notes.begin(), track.notes.end(),
                [](const auto &a, const auto &b) { return a.key < b.key; });
            info.rangeText = QStringLiteral("%1 - %2")
                                 .arg(toneNumToToneName(minMaxNotes.first->key),
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

int MidiConverter::midiImportHandler() {
    const int ret = showImportDialog();
    return ret == 1 ? AppendToProject : ret == 2 ? NewProject : -1;
}

bool MidiConverter::load(const QString &path, AppModel *model, QString &errMsg,
                         const ImportMode mode) {
    const auto midiConverter = std::make_unique<opendspx::MidiConverter>();
    const QString language = appOptions->general()->defaultSingingLanguage;

    QFile midiFile(path);
    if (!midiFile.open(QIODevice::ReadOnly)) {
        showErrorDialog("Error", QString("Failed to read midi file.\npath: %1").arg(path));
        return false;
    }

    const QByteArray midiData = midiFile.readAll();
    opendspx::MidiConverter::Error midiError;
    std::stringstream ss(midiData.toStdString(), std::ios::in);
    auto midiMediate = midiConverter->convertMidiToIntermediate(ss, midiError, {true});
    if (midiError != opendspx::MidiConverter::Error::NoError) {
        showErrorDialog("Error", QString("Failed to load midi file.\npath: %1\ntype: %2")
                                     .arg(path)
                                     .arg(static_cast<int>(midiError)));
        return false;
    }

    MidiConverterDialog dlg(buildTrackInfoList(midiMediate.tracks()), Dialog::globalParent());
    dlg.detectCodec();

    QObject::connect(&dlg, &MidiConverterDialog::separateMidiChannelsChanged, &dlg,
                     [&](bool enabled) {
                         std::stringstream ssUpdate(midiData.toStdString(), std::ios::in);
                         auto updated =
                             midiConverter->convertMidiToIntermediate(ssUpdate, midiError,
                                                                      {enabled});
                         if (midiError != opendspx::MidiConverter::Error::NoError)
                             return;
                         midiMediate = std::move(updated);
                         dlg.setTrackInfoList(buildTrackInfoList(midiMediate.tracks()));
                         dlg.detectCodec();
                     });

    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }

    const auto codec = dlg.selectedCodec();
    const auto selectTrackIds = dlg.selectedTracks();

    std::vector<opendspx::MidiIntermediateData::Track> selectedTracks;
    selectedTracks.reserve(selectTrackIds.size());
    for (const auto index : selectTrackIds) {
        selectedTracks.push_back(midiMediate.tracks().at(index));
    }

    auto decodeText = [&](const std::string &value) -> std::string {
        if (value.empty())
            return {};
        const auto decoded = MidiTextCodecConverter::decode(QByteArray::fromStdString(value),
                                                            codec);
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
        dlg.importTempo() ? midiMediate.tempos()
                          : std::vector<opendspx::MidiIntermediateData::Tempo>{},
        dlg.importTimeSignature() ? midiMediate.timeSignatures()
                                  : std::vector<opendspx::MidiIntermediateData::TimeSignature>{},
        midiMediate.markers(),
        selectedTracks,
    };

    const auto midiDspx = midiConverter->convertIntermediateToDspx(midiMediate);

    const auto &timeline = midiDspx.content.timeline;
    const auto hasTimeSignature = !timeline.timeSignatures.empty();
    const auto hasTempo = !timeline.tempos.empty();

    if (hasTimeSignature) {
        const auto &ts = timeline.timeSignatures.front();
        if (ts.denominator != 2 && ts.denominator != 4 && ts.denominator != 8 &&
            ts.denominator != 16) {
            showErrorDialog("Warning",
                            QString("Failed to load midi file.\ntimeSignatures denominator "
                                    "must be: 2, 4, 8, 16\ncurrent denominator: %1")
                                .arg(ts.denominator));
            return false;
        }
    }

    if (mode == NewProject) {
        model->newProject();
        model->clearTracks();
    } else if (mode != AppendToProject) {
        return false;
    }

    if (hasTimeSignature) {
        const auto &ts = timeline.timeSignatures.front();
        if (model->timeSignature().numerator != ts.numerator ||
            model->timeSignature().denominator != ts.denominator) {
            model->setTimeSignature({ts.numerator, ts.denominator});
        }
    }

    if (hasTempo) {
        const auto &tempoVal = timeline.tempos.front().value;
        if (qAbs(model->tempo() - tempoVal) > 0.001)
            model->setTempo(tempoVal);
    }

    if (!midiDspx.content.tracks.empty()) {
        convertTracks(midiDspx, selectTrackIds, model, language);
        return true;
    }
    return false;
}

bool MidiConverter::save(const QString &path, AppModel *model, QString &errMsg) {
    opendspx::Model dspx;
    opendspx::MidiConverter midiConverter;

    dspx.content.timeline.tempos.push_back({0, model->tempo()});
    const auto &ts = model->timeSignature();
    dspx.content.timeline.timeSignatures.push_back({0, ts.numerator, ts.denominator});

    encodeTracks(model, dspx);

    auto midiMediate = midiConverter.convertDspxToIntermediate(dspx);
    std::stringstream ss(std::ios::out);
    midiConverter.convertIntermediateToMidi(ss, midiMediate);

    auto saveMidiToFile = [](const QByteArray &midi, const QString &filePath,
                             QString &msg) -> bool {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            msg += "Failed to open file for writing:" + filePath;
            return false;
        }

        const qint64 written = file.write(midi);
        file.close();

        if (written != midi.size()) {
            msg += "Failed to write all data to file:" + filePath;
            return false;
        }

        return true;
    };

    QString msg;
    const auto result = saveMidiToFile(QByteArray::fromStdString(ss.str()), path, msg);
    if (!result) {
        showErrorDialog(
            "Warning",
            QString("Failed to save midi file.\n path: %1\nerror: %2").arg(path).arg(msg));
        return false;
    }
    return true;
}