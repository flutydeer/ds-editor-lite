//
// Created by fluty on 2024/1/27.
//

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QTextCodec>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>

#include "AppModel.h"

#include "Utils/IdGenerator.h"
#include "opendspx/qdspxtrack.h"
#include "opendspx/qdspxtimeline.h"
#include "opendspx/qdspxmodel.h"
#include "opendspx/converters/midi.h"

int AppModel::numerator() const {
    return m_numerator;
}
void AppModel::setNumerator(int numerator) {
    qDebug() << "AppModel::setNumerator" << numerator;
    m_numerator = numerator;
    emit timeSignatureChanged(m_numerator, m_denominator);
}
int AppModel::denominator() const {
    return m_denominator;
}
void AppModel::setDenominator(int denominator) {
    qDebug() << "AppModel::setDenominator";
    m_denominator = denominator;
    emit timeSignatureChanged(m_numerator, m_denominator);
}
double AppModel::tempo() const {
    return m_tempo;
}
void AppModel::setTempo(double tempo) {
    qDebug() << "AppModel::setTempo" << tempo;
    m_tempo = tempo;
    emit tempoChanged(m_tempo);
}
const QList<DsTrack *> &AppModel::tracks() const {
    return m_tracks;
}

void AppModel::insertTrack(DsTrack *track, int index) {
    connect(track, &DsTrack::propertyChanged, this, [=] {
        auto trackIndex = m_tracks.indexOf(track);
        emit tracksChanged(PropertyUpdate, trackIndex, track);
    });
    m_tracks.insert(index, track);
    emit tracksChanged(Insert, index, track);
}
void AppModel::removeTrack(int index) {
    auto track = m_tracks[index];
    onSelectedClipChanged(-1, -1);
    m_tracks.removeAt(index);
    emit tracksChanged(Remove, index, track);
}
void AppModel::newProject() {
    reset();
    emit modelChanged();
}

bool trackSelector(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList,
                   const QList<QByteArray> &labelList, QList<int> *selectIDs, QTextCodec *codec) {

    // Set UTF-8 as the text codec
    codec = QTextCodec::codecForName("UTF-8");

    // Create a dialog
    QDialog dialog;
    dialog.setWindowTitle("MIDI Track Selector");

    // Create a layout for the dialog
    auto *layout = new QVBoxLayout(&dialog);

    // Create checkboxes for each MIDI track
    QList<QCheckBox *> checkBoxes;
    for (const auto &trackInfo : trackInfoList) {
        auto *checkBox = new QCheckBox(QString("track %1:  %2 notes")
                                           .arg(trackInfo.title.constData())
                                           .arg(trackInfo.lyrics.count()));
        checkBox->setChecked(false);
        checkBoxes.append(checkBox);
        layout->addWidget(checkBox);
    }

    // Create OK and Cancel buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);

    // Connect the button signals to slots on the dialog
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Execute the dialog and get the result
    int result = dialog.exec();

    // Process the selected MIDI tracks
    if (result == QDialog::Accepted) {
        selectIDs->clear();
        for (int i = 0; i < checkBoxes.size(); ++i) {
            if (checkBoxes.at(i)->isChecked()) {
                selectIDs->append(i);
            }
        }
        return true;
    } else {
        // User canceled the dialog
        return false;
    }
}


int midiImportHandler() {
    QMessageBox msgBox;
    msgBox.setText("MIDI Import");
    msgBox.setInformativeText("Do you want to create a new track or use a new project?");
    QPushButton *newTrackButton = msgBox.addButton("New Track", QMessageBox::ActionRole);
    QPushButton *newProjectButton = msgBox.addButton("New Project", QMessageBox::ActionRole);
    msgBox.addButton("Cancel", QMessageBox::RejectRole);
    msgBox.exec();

    QAbstractButton *clickedButton = msgBox.clickedButton();
    auto *clickedPushButton = qobject_cast<QPushButton *>(clickedButton);
    if (clickedPushButton == newTrackButton) {
        return 0;
    } else if (clickedPushButton == newProjectButton) {
        return 1;
    } else {
        return -1;
    }
}

bool AppModel::importMidiFile(const QString &filename) {
    int midiImport = midiImportHandler();

    auto dspx = new QDspx::Model;
    std::function<bool(const QList<QDspx::MidiConverter::TrackInfo> &, const QList<QByteArray> &,
                       QList<int> *, QTextCodec *)>
        midiSelector = trackSelector;
    QVariantMap args = {};
    args.insert(QStringLiteral("selector"),
                QVariant::fromValue(reinterpret_cast<quintptr>(&midiSelector)));

    auto midi = new QDspx::MidiConverter;

    auto decodeNotes = [](const QList<QDspx::Note> &arrNotes) {
        QList<DsNote> notes;
        for (const QDspx::Note &dsNote : arrNotes) {
            DsNote note;
            note.setStart(dsNote.pos);
            note.setLength(dsNote.length);
            note.setKeyIndex(dsNote.keyNum);
            note.setLyric(dsNote.lyric);
            note.setPronunciation("la");
            notes.append(note);
        }
        return notes;
    };

    auto decodeClips = [&](const QDspx::Track &track, DsTrack *dsTack) {
        for (auto &clip : track.clips) {
            if (clip->type == QDspx::Clip::Type::Singing) {
                auto singClip = clip.dynamicCast<QDspx::SingingClip>();
                auto singingClip = new DsSingingClip;
                singingClip->setName(clip->name);
                singingClip->setStart(clip->time.start);
                singingClip->setClipStart(clip->time.clipStart);
                singingClip->setLength(clip->time.length);
                singingClip->setClipLen(clip->time.clipLen);
                singingClip->notes.append(decodeNotes(singClip->notes));
                dsTack->insertClip(singingClip);
            } else if (clip->type == QDspx::Clip::Type::Audio) {
                auto audioClip = new DsAudioClip;
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

    auto decodeTracks = [&](const QDspx::Model *dspx, QList<DsTrack *> &tracks) {
        for (int i = 0; i < dspx->content.tracks.count(); i++) {
            auto track = dspx->content.tracks[i];
            auto dsTrack = new DsTrack;
            dsTrack->setName(track.name);
            decodeClips(track, dsTrack);
            tracks.append(dsTrack);
        }
    };

    auto returnCode = midi->load(filename, dspx, args);

    if (returnCode.type != QDspx::ReturnCode::Success) {
        QMessageBox::warning(nullptr, "Warning",
                             QString("Failed to load midi file.\r\npath: %1\r\ntype: %2 code: %3")
                                 .arg(filename)
                                 .arg(returnCode.type)
                                 .arg(returnCode.code));
        return false;
    }

    if (midiImport == 1) {
        reset();
    } else if (midiImport == 0) {
        if (m_numerator != dspx->content.timeline.timeSignatures[0].num ||
            m_denominator != dspx->content.timeline.timeSignatures[0].den) {
            QMessageBox msgBox;
            msgBox.setText("Time Signature Mismatch");
            msgBox.setInformativeText("The time signature of the MIDI file does not match the "
                                      "current project. Do you want to continue?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::No) {
                return false;
            }
        } else if (m_tempo != dspx->content.timeline.tempos[0].value) {
            QMessageBox msgBox;
            msgBox.setText("Tempo Mismatch");
            msgBox.setInformativeText("The tempo of the MIDI file does not match the current "
                                      "project. Do you want to continue?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::Yes);
            int ret = msgBox.exec();
            if (ret == QMessageBox::No) {
                return false;
            }
        }
    } else {
        return false;
    }

    auto timeline = dspx->content.timeline;
    m_numerator = timeline.timeSignatures[0].num;
    m_denominator = timeline.timeSignatures[0].den;
    m_tempo = timeline.tempos[0].value;
    decodeTracks(dspx, m_tracks);
    emit modelChanged();
    return true;
}

bool midiOverlapHandler() {
    QMessageBox msgBox;
    msgBox.setText("MIDI Overlap");
    msgBox.setInformativeText("The MIDI file contains overlapping notes. Do you want to continue?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    if (ret == QMessageBox::Yes) {
        return true;
    } else {
        return false;
    }
}

bool AppModel::exportMidiFile(const QString &filename) {
    QDspx::Model dspx;
    std::function<bool()> midiOverlap = midiOverlapHandler;
    QVariantMap args = {};
    args.insert(QStringLiteral("overlapHandler"),
                QVariant::fromValue(reinterpret_cast<quintptr>(&midiOverlap)));
    auto midi = new QDspx::MidiConverter;

    auto encodeNotes = [](const QList<DsNote> &notes) {
        QList<QDspx::Note> arrNotes;
        for (const auto &note : notes) {
            QDspx::Note dsNote;
            dsNote.pos = note.start();
            dsNote.length = note.length();
            dsNote.keyNum = note.keyIndex();
            dsNote.lyric = note.lyric();
            arrNotes.append(dsNote);
        }
        return arrNotes;
    };

    auto encodeClips = [&](const DsTrack *dsTrack, QDspx::Track *track) {
        for (const auto &clip : dsTrack->clips()) {
            if (clip->type() == DsClip::Singing) {
                auto singingClip = dynamic_cast<DsSingingClip *>(clip);
                auto singClip = QDspx::SingingClipRef::create();
                singClip->name = clip->name();
                singClip->time.start = clip->start();
                singClip->time.clipStart = clip->clipStart();
                singClip->time.length = clip->length();
                singClip->time.clipLen = clip->clipLen();
                singClip->notes = encodeNotes(singingClip->notes);
                track->clips.append(singClip);
            } else if (clip->type() == DsClip::Audio) {
                auto audioClip = dynamic_cast<DsAudioClip *>(clip);
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

    auto encodeTracks = [&](const QList<DsTrack *> &tracks, QDspx::Model &dspx) {
        for (const auto &dsTrack : tracks) {
            QDspx::Track track;
            track.name = dsTrack->name();
            encodeClips(dsTrack, &track);
            dspx.content.tracks.append(track);
        }
    };

    auto timeline = new QDspx::Timeline;

    timeline->tempos.append(QDspx::Tempo(0, m_tempo));
    timeline->timeSignatures.append(QDspx::TimeSignature(0, m_numerator, m_denominator));
    dspx.content.timeline = *timeline;

    encodeTracks(m_tracks, dspx);
    auto returnCode = midi->save(filename, dspx, args);

    if (returnCode.type != QDspx::ReturnCode::Success) {
        QMessageBox::warning(nullptr, "Warning",
                             QString("Failed to save midi file.\r\npath: %1\r\ntype: %2 code: %3")
                                 .arg(filename)
                                 .arg(returnCode.type)
                                 .arg(returnCode.code));
        return false;
    }

    return true;
}

bool AppModel::loadAProject(const QString &filename) {
    reset();

    auto openJsonFile = [](const QString &filename, QJsonObject *jsonObj) {
        QFile loadFile(filename);
        if (!loadFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open project file";
            return false;
        }
        QByteArray allData = loadFile.readAll();
        loadFile.close();
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(allData, &err);
        if (err.error != QJsonParseError::NoError)
            return false;
        if (json.isObject()) {
            *jsonObj = json.object();
        }
        return true;
    };

    auto decodeNotes = [](const QJsonArray &arrNotes) {
        QList<DsNote> notes;
        for (const auto valNote : qAsConst(arrNotes)) {
            auto objNote = valNote.toObject();
            DsNote note;
            note.setStart(objNote.value("pos").toInt());
            note.setLength(objNote.value("dur").toInt());
            note.setKeyIndex(objNote.value("pitch").toInt());
            note.setLyric(objNote.value("lyric").toString());
            note.setPronunciation("la");
            notes.append(note);
        }
        return notes;
    };

    auto decodeClips = [&](const QJsonArray &arrClips, DsTrack *dsTack, const QString &type,
                           int trackIndex) {
        for (const auto &valClip : qAsConst(arrClips)) {
            auto objClip = valClip.toObject();
            if (type == "sing") {
                auto singingClip = new DsSingingClip;
                singingClip->setName("Clip");
                singingClip->setStart(objClip.value("pos").toInt());
                singingClip->setClipStart(objClip.value("clipPos").toInt());
                singingClip->setLength(objClip.value("dur").toInt());
                singingClip->setClipLen(objClip.value("clipDur").toInt());
                auto arrNotes = objClip.value("notes").toArray();
                singingClip->notes.append(decodeNotes(arrNotes));
                dsTack->insertClip(singingClip);
            } else if (type == "audio") {
                auto audioClip = new DsAudioClip;
                audioClip->setName("Clip");
                audioClip->setStart(objClip.value("pos").toInt());
                audioClip->setClipStart(objClip.value("clipPos").toInt());
                audioClip->setLength(objClip.value("dur").toInt());
                audioClip->setClipLen(objClip.value("clipDur").toInt());
                audioClip->setPath(objClip.value("path").toString());
                dsTack->insertClip(audioClip);
            }
        }
    };

    auto decodeTracks = [&](const QJsonArray &arrTracks, QList<DsTrack *> &tracks) {
        int i = 0;
        for (const auto &valTrack : qAsConst(arrTracks)) {
            auto objTrack = valTrack.toObject();
            auto type = objTrack.value("type").toString();
            auto track = new DsTrack;
            decodeClips(objTrack.value("patterns").toArray(), track, type, i);
            tracks.append(track);
            i++;
        }
    };

    QJsonObject objAProject;
    if (openJsonFile(filename, &objAProject)) {
        m_numerator = objAProject.value("beatsPerBar").toInt();
        m_tempo = objAProject.value("tempos").toArray().first().toObject().value("bpm").toDouble();
        decodeTracks(objAProject.value("tracks").toArray(), m_tracks);
        // auto clip = tracks().first().clips.first().dynamicCast<DsSingingClip>();
        // qDebug() << clip->notes.count();
        emit modelChanged();
        return true;
    }
    return false;
}
void AppModel::onSelectedClipChanged(int trackIndex, int clipId) {
    qDebug() << "AppModel::onSelectedClipChanged" << trackIndex << clipId;
    m_selectedClipTrackIndex = trackIndex;
    m_selectedClipId = clipId;
    emit selectedClipChanged(m_selectedClipTrackIndex, m_selectedClipId);
}
void AppModel::reset() {
    m_tempo = 120;
    m_numerator = 4;
    m_denominator = 4;
    m_tracks.clear();
}