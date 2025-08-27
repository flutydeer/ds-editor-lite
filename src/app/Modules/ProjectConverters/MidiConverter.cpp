#include "MidiConverter.h"
#include "MidiConverterDialog.h"

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include "Model/AppModel/Track.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppOptions/AppOptions.h"

#include "Utils/G2pUtil.h"

#include <opendspx/qdspxtrack.h>
#include <opendspx/qdspxtimeline.h>
#include <opendspx/qdspxmodel.h>
#include <opendspx/converters/midi.h>

#include <QTextCodec>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QTextEdit>
#include <utility>

bool trackSelector(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList,
                   const QList<QByteArray> &labelList, QList<int> *selectIDs, QTextCodec **codec) {
    MidiConverterDialog dlg(trackInfoList, Dialog::globalParent());
    if (dlg.exec()) {
        *selectIDs = dlg.selectedTracks();
        if (dlg.selectedCodec()) {
            *codec = dlg.selectedCodec();
        }
        return true;
    }
    return false;
}

bool showOverlapDialog() {
    MessageDialog msgBox;
    msgBox.setTitle("MIDI Overlap");
    msgBox.setMessage("The MIDI file contains overlapping notes. Do you want to continue?");
    msgBox.addAccentButton("Yes", 1);
    msgBox.addButton("No", 0);
    return msgBox.exec() == 1;
}

int showImportDialog() {
    MessageDialog msgBox;
    msgBox.setWindowTitle("MIDI Import");
    msgBox.setMessage("Do you want to create a new track or use a new project?");
    msgBox.addButton("New Track", 1);
    msgBox.addButton("New Project", 2);
    msgBox.addButton("Cancel", 0);
    return msgBox.exec();
}

void showErrorDialog(const QString &title, const QString &message) {
    MessageDialog msgBox;
    msgBox.setWindowTitle(title);
    msgBox.setMessage(message);
    msgBox.addAccentButton("Ok", 1);
    msgBox.exec();
}

QList<Note *> convertNotes(const QList<QDspx::Note> &arrNotes, const int offset,
                           const QString &language) {
    QList<Note *> notes;
    for (const QDspx::Note &dsNote : arrNotes) {
        const auto note = new Note;
        note->setLocalStart(dsNote.pos - offset);
        note->setLength(dsNote.length);
        note->setKeyIndex(dsNote.keyNum);
        note->setLyric(dsNote.lyric.isEmpty() ? appOptions->general()->defaultLyric : dsNote.lyric);
        note->setLanguage(language);
        notes.append(note);
    }
    return notes;
}

void convertClips(const QDspx::Track &track, Track *dsTrack, const QString &language) {
    for (auto &clip : track.clips) {
        if (clip->type == QDspx::Clip::Type::Singing) {
            const auto singClip = clip.dynamicCast<QDspx::SingingClip>();
            const auto singingClip = new SingingClip;
            singingClip->setName(clip->name);
            singingClip->setStart(clip->time.start);
            singingClip->setClipStart(clip->time.clipStart);
            singingClip->setLength(clip->time.length);
            singingClip->setClipLen(clip->time.clipLen + 960);
            singingClip->defaultLanguage = language;
            singingClip->defaultG2pId = g2pIdFromLanguage(language);

            auto notes = convertNotes(singClip->notes, singClip->time.start, language);
            for (const auto note : notes) {
                singingClip->insertNote(note);
            }
            dsTrack->insertClip(singingClip);
        } else if (clip->type == QDspx::Clip::Type::Audio) {
            const auto audioClip = new AudioClip;
            audioClip->setName(clip->name);
            audioClip->setStart(clip->time.start);
            audioClip->setClipStart(clip->time.clipStart);
            audioClip->setLength(clip->time.length);
            audioClip->setClipLen(clip->time.clipLen);
            audioClip->setPath(clip.dynamicCast<QDspx::AudioClip>()->path);
            dsTrack->insertClip(audioClip);
        }
    }
}

void convertTracks(const QDspx::Model &dspx, AppModel *model, const QString &language) {
    for (int i = 0; i < dspx.content.tracks.count(); ++i) {
        const auto &track = dspx.content.tracks[i];
        const auto dsTrack = new Track;
        dsTrack->setName(track.name);
        dsTrack->setDefaultLanguage(language);
        dsTrack->setDefaultG2pId(g2pIdFromLanguage(language));
        convertClips(track, dsTrack, language);
        model->insertTrack(dsTrack, i);
    }
}

QList<QDspx::Note> encodeNotes(const OverlappableSerialList<Note> &notes) {
    QList<QDspx::Note> arrNotes;
    for (const auto &note : notes) {
        QDspx::Note dsNote;
        dsNote.pos = note->globalStart();
        dsNote.length = note->length();
        dsNote.keyNum = note->keyIndex();
        dsNote.lyric = note->lyric();
        arrNotes.append(dsNote);
    }
    return arrNotes;
}

void encodeClips(const Track *dsTrack, QDspx::Track *track) {
    for (const auto &clip : dsTrack->clips()) {
        if (clip->clipType() == Clip::Singing) {
            const auto singingClip = dynamic_cast<SingingClip *>(clip);
            auto singClip = QDspx::SingingClipRef::create();
            singClip->name = clip->name();
            singClip->time = {clip->start(), clip->length(), clip->clipStart(), clip->clipLen()};
            singClip->notes = encodeNotes(singingClip->notes());
            track->clips.append(singClip);
        } else if (clip->clipType() == Clip::Audio) {
            const auto audioClip = dynamic_cast<AudioClip *>(clip);
            auto audioClipRef = QDspx::AudioClipRef::create();
            audioClipRef->name = clip->name();
            audioClipRef->time = {clip->start(), clip->length(), clip->clipStart(),
                                  clip->clipLen()};
            audioClipRef->path = audioClip->path();
            track->clips.append(audioClipRef);
        }
    }
}

void encodeTracks(const AppModel *model, QDspx::Model &dspx) {
    for (const auto &dsTrack : model->tracks()) {
        QDspx::Track track;
        track.name = dsTrack->name();
        encodeClips(dsTrack, &track);
        dspx.content.tracks.append(track);
    }
}

MidiConverter::MidiConverter(TimeSignature timeSignature, const double tempo)
    : m_timeSignature(std::move(timeSignature)), m_tempo(tempo) {
}

int MidiConverter::midiImportHandler() {
    const int ret = showImportDialog();
    return ret == 1 ? AppendToProject : ret == 2 ? NewProject : -1;
}

bool MidiConverter::load(const QString &path, AppModel *model, QString &errMsg,
                         const ImportMode mode) {
    const auto dspx = std::make_unique<QDspx::Model>();
    const auto midi = std::make_unique<QDspx::MidiConverter>();
    const QString language = appOptions->general()->defaultSingingLanguage;

    QVariantMap args;
    std::function selector = trackSelector;
    args.insert("selector", QVariant::fromValue(reinterpret_cast<quintptr>(&selector)));

    const auto result = midi->load(path, dspx.get(), args);
    if (result.type != QDspx::Result::Success) {
        showErrorDialog("Warning", QString("Failed to load midi file.\npath: %1\ntype: %2 code: %3")
                                       .arg(path)
                                       .arg(result.type)
                                       .arg(result.code));
        return false;
    }

    if (mode == NewProject) {
        model->newProject();
        model->clearTracks();
    } else if (mode != AppendToProject) {
        return false;
    }

    const auto &timeline = dspx->content.timeline;
    const auto &ts = timeline.timeSignatures.first();
    const auto &tempoVal = timeline.tempos.first().value;

    if (m_timeSignature.numerator != ts.num || m_timeSignature.denominator != ts.den)
        model->setTimeSignature({ts.num, ts.den});

    if (qAbs(m_tempo - tempoVal) > 0.001)
        model->setTempo(tempoVal);

    if (!dspx->content.tracks.isEmpty()) {
        convertTracks(*dspx, model, language);
        return true;
    }
    return false;
}

bool MidiConverter::save(const QString &path, AppModel *model, QString &errMsg) {
    QDspx::Model dspx;
    const auto midi = std::make_unique<QDspx::MidiConverter>();

    QVariantMap args;
    std::function overlapHandler = showOverlapDialog;
    args.insert("overlapHandler", QVariant::fromValue(reinterpret_cast<quintptr>(&overlapHandler)));

    dspx.content.timeline.tempos.append({0, model->tempo()});
    const auto &ts = model->timeSignature();
    dspx.content.timeline.timeSignatures.append({0, ts.numerator, ts.denominator});

    encodeTracks(model, dspx);
    const auto result = midi->save(path, dspx, args);

    if (result.type != QDspx::Result::Success) {
        showErrorDialog("Warning",
                        QString("Failed to save midi file.\n path: %1\ntype: %2 code: %3")
                            .arg(path)
                            .arg(result.type)
                            .arg(result.code));
        return false;
    }
    return true;
}