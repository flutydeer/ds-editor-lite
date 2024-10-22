#include "MidiConverter.h"

#include "MidiConverterDialog.h"
#include "Model/AppModel/AudioClip.h"
#include "opendspx/qdspxtrack.h"
#include "opendspx/qdspxtimeline.h"
#include "opendspx/qdspxmodel.h"
#include "opendspx/converters/midi.h"

#include <QTextCodec>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QListWidget>

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include "Model/AppModel/Track.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "UI/Controls/ComboBox.h"

#include <QTextEdit>
#include <utility>
#include <utility>

static bool trackSelector(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList,
                          const QList<QByteArray> &labelList, QList<int> *selectIDs,
                          QTextCodec **codec) {
    MidiConverterDialog dlg(trackInfoList);
    // TODO: export tempo
    if (dlg.exec()) {
        *selectIDs = dlg.selectedTracks();
        if (dlg.selectedCodec() != nullptr)
            *codec = dlg.selectedCodec();
        return true;
    } else {
        // User canceled the dialog
        return false;
    }
}

MidiConverter::MidiConverter(TimeSignature timeSignature, const double tempo)
    : m_timeSignature(std::move(std::move(timeSignature))), m_tempo(tempo) {
}

int MidiConverter::midiImportHandler() {
    MessageDialog msgBox;
    msgBox.setWindowTitle("MIDI Import");
    msgBox.setMessage("Do you want to create a new track or use a new project?");
    msgBox.addButton("New Track", 1);
    msgBox.addButton("New Project", 2);
    msgBox.addButton("Cancel", 0);
    const int ret = msgBox.exec();

    if (ret == 1) {
        return ImportMode::AppendToProject;
    } else if (ret == 2) {
        return ImportMode::NewProject;
    } else {
        return -1;
    }
}

static bool midiOverlapHandler() {
    MessageDialog msgBox;
    msgBox.setTitle("MIDI Overlap");
    msgBox.setMessage("The MIDI file contains overlapping notes. Do you want to continue?");
    msgBox.addAccentButton("Yes", 1);
    msgBox.addButton("No", 0);
    const int ret = msgBox.exec();
    if (ret == 1) {
        return true;
    } else {
        return false;
    }
}

bool MidiConverter::load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode) {
    const auto dspx = new QDspx::Model;
    std::function<bool(const QList<QDspx::MidiConverter::TrackInfo> &, const QList<QByteArray> &,
                       QList<int> *, QTextCodec **)>
        midiSelector = trackSelector;
    QVariantMap args = {};
    args.insert(QStringLiteral("selector"),
                QVariant::fromValue(reinterpret_cast<quintptr>(&midiSelector)));

    const auto midi = new QDspx::MidiConverter();

    auto decodeNotes = [](const QList<QDspx::Note> &arrNotes) {
        QList<Note *> notes;
        for (const QDspx::Note &dsNote : arrNotes) {
            const auto note = new Note;
            note->setStart(dsNote.pos);
            note->setLength(dsNote.length);
            note->setKeyIndex(dsNote.keyNum);
            note->setLyric(dsNote.lyric);
            notes.append(note);
        }
        return notes;
    };

    auto decodeClips = [&](const QDspx::Track &track, Track *dsTack) {
        for (auto &clip : track.clips) {
            if (clip->type == QDspx::Clip::Type::Singing) {
                const auto singClip = clip.dynamicCast<QDspx::SingingClip>();
                const auto singingClip = new SingingClip;
                singingClip->setName(clip->name);
                singingClip->setStart(clip->time.start);
                singingClip->setClipStart(clip->time.clipStart);
                singingClip->setLength(clip->time.length);
                singingClip->setClipLen(clip->time.clipLen + 960);
                auto notes = decodeNotes(singClip->notes);
                for (const auto &note : notes)
                    singingClip->insertNote(note);
                dsTack->insertClip(singingClip);
            } else if (clip->type == QDspx::Clip::Type::Audio) {
                const auto audioClip = new AudioClip;
                audioClip->setName(clip->name);
                audioClip->setStart(clip->time.start);
                audioClip->setClipStart(clip->time.clipStart);
                audioClip->setLength(clip->time.length);
                audioClip->setClipLen(clip->time.clipLen);
                audioClip->setPath(clip.dynamicCast<QDspx::AudioClip>()->path);
                dsTack->insertClip(audioClip);
            }
        }
    };

    auto decodeTracks = [&](const QDspx::Model *_dspx, AppModel *_model) {
        for (int i = 0; i < _dspx->content.tracks.count(); i++) {
            auto track = _dspx->content.tracks[i];
            const auto dsTrack = new Track;
            dsTrack->setName(track.name);
            decodeClips(track, dsTrack);
            _model->insertTrack(dsTrack, i);
        }
    };

    const auto returnCode = midi->load(path, dspx, args);

    if (returnCode.type != QDspx::Result::Success) {
        Dialog msgDlg;
        msgDlg.setWindowTitle("Warning");
        msgDlg.setMessage(QString("Failed to load midi file.\r\npath: %1\r\ntype: %2 code: %3")
                              .arg(path)
                              .arg(returnCode.type)
                              .arg(returnCode.code));
        return false;
    }

    if (mode == ImportMode::NewProject) {
        model->newProject();
        model->clearTracks();
    } else if (mode == ImportMode::AppendToProject) {
    } else {
        return false;
    }

    if (m_timeSignature.numerator != dspx->content.timeline.timeSignatures[0].num ||
    m_timeSignature.denominator != dspx->content.timeline.timeSignatures[0].den) {
        MessageDialog msgBox;
        msgBox.setWindowTitle("Time Signature Mismatch");
        msgBox.setMessage("The timeSignature of the MIDI file does not match the "
                          "current project. Do you want to use MIDI's timeSignature?");
        msgBox.addAccentButton("Yes", 1);
        msgBox.addButton("No", 0);
        if (msgBox.exec() == 1)
            model->setTimeSignature(
                TimeSignature(dspx->content.timeline.timeSignatures[0].num,
                              dspx->content.timeline.timeSignatures[0].den));
        else
            model->setTimeSignature(m_timeSignature);
    }
    if (m_tempo != std::round(dspx->content.timeline.tempos[0].value * 1000) / 1000) {
        MessageDialog msgBox;
        msgBox.setWindowTitle("Tempo Mismatch");
        msgBox.setMessage("The tempo of the MIDI file does not match the current "
                          "project. Do you want to use MIDI's tempo?");
        msgBox.addAccentButton("Yes", 1);
        msgBox.addButton("No", 0);
        if (msgBox.exec() == 1)
            model->setTempo(std::round(dspx->content.timeline.tempos[0].value * 1000 / 1000));
        else
            model->setTempo(m_tempo);
    }

    if (dspx->content.tracks.count() > 0) {
        decodeTracks(dspx, model);
        return true;
    }
    return false;
}

bool MidiConverter::save(const QString &path, AppModel *model, QString &errMsg) {
    QDspx::Model dspx;
    std::function<bool()> midiOverlap = midiOverlapHandler;
    QVariantMap args = {};
    args.insert(QStringLiteral("overlapHandler"),
                QVariant::fromValue(reinterpret_cast<quintptr>(&midiOverlap)));
    const auto midi = new QDspx::MidiConverter;

    auto encodeNotes = [](const OverlappableSerialList<Note> &notes) {
        QList<QDspx::Note> arrNotes;
        for (const auto &note : notes) {
            QDspx::Note dsNote;
            dsNote.pos = note->start();
            dsNote.length = note->length();
            dsNote.keyNum = note->keyIndex();
            dsNote.lyric = note->lyric();
            arrNotes.append(dsNote);
        }
        return arrNotes;
    };

    auto encodeClips = [&](const Track *dsTrack, QDspx::Track *track) {
        for (const auto &clip : dsTrack->clips()) {
            if (clip->clipType() == Clip::Singing) {
                const auto singingClip = dynamic_cast<SingingClip *>(clip);
                auto singClip = QDspx::SingingClipRef::create();
                singClip->name = clip->name();
                singClip->time.start = clip->start();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->notes = encodeNotes(singingClip->notes());
                track->clips.append(singClip);
            } else if (clip->clipType() == Clip::Audio) {
                const auto audioClip = dynamic_cast<AudioClip *>(clip);
                auto audioClipRef = QDspx::AudioClipRef::create();
                audioClipRef->name = clip->name();
                audioClipRef->time.start = clip->start();
                audioClipRef->time.clipStart = clip->clipStart();
                audioClipRef->time.length = clip->length();
                audioClipRef->time.clipLen = clip->clipLen();
                audioClipRef->path = audioClip->path();
                track->clips.append(audioClipRef);
            }
        }
    };

    auto encodeTracks = [&](const AppModel *_model, QDspx::Model &_dspx) {
        for (const auto &dsTrack : _model->tracks()) {
            QDspx::Track track;
            track.name = dsTrack->name();
            encodeClips(dsTrack, &track);
            _dspx.content.tracks.append(track);
        }
    };

    const auto timeline = new QDspx::Timeline;

    timeline->tempos.append(QDspx::Tempo(0, model->tempo()));
    timeline->timeSignatures.append(QDspx::TimeSignature(0, model->timeSignature().numerator,
                                                         model->timeSignature().denominator));
    dspx.content.timeline = *timeline;

    encodeTracks(model, dspx);
    const auto returnCode = midi->save(path, dspx, args);

    if (returnCode.type != QDspx::Result::Success) {
        Dialog msgDlg;
        msgDlg.setTitle("Warning");
        msgDlg.setMessage(QString("Failed to save midi file.\r\npath: %1\r\ntype: %2 code: %3")
                              .arg(path)
                              .arg(returnCode.type)
                              .arg(returnCode.code));
        return false;
    }
    return true;
}
