#ifndef DS_EDITOR_LITE_MIDICONVERTER_H
#define DS_EDITOR_LITE_MIDICONVERTER_H

#include <lite/ProjectConverters/IProjectConverter.h>

#include <opendspxconverter/midi/midiintermediatedata.h>

#include <QByteArray>
#include <QList>
#include <QString>

using ImportMode = IProjectConverter::ImportMode;

// One MIDI track as presented to the interactive import UI.
struct MidiImportTrackInfo {
    QByteArray name;
    QString rangeText;
    int noteCount = 0;
    QList<QByteArray> lyrics;
    bool disabled = false;
    bool selectedByDefault = false;
};

// The choices the interactive import UI hands back to the converter.
struct MidiImportOptions {
    QByteArray codec;
    QList<int> selectedTrackIndices;
    bool importTempo = false;
    bool importTimeSignature = false;
};

// Back-channel the interactive UI uses to re-derive the track list when the
// user toggles "separate MIDI channels" — the converter re-parses the file and
// returns the new track layout.
class MidiTrackReconverter {
public:
    virtual ~MidiTrackReconverter() = default;
    virtual QList<MidiImportTrackInfo> reconvert(bool separateChannels) = 0;
};

// Shared helper: builds the UI-facing track list from parsed intermediate
// data. Used by MidiFileParser and by reconvert-on-channel-toggle flows.
QList<MidiImportTrackInfo>
    buildMidiTrackInfoList(const std::vector<opendspx::MidiIntermediateData::Track> &tracks);

class MidiConverter : public IProjectConverter {
public:
    enum class LoadStatus { Success, Canceled, Failed };

    struct LoadOptions {
        bool importTempo = false;
        bool importTimeSignature = false;
    };

    explicit MidiConverter();
    bool load(const QString &path, AppModel *model, QString &errMsg, ImportMode mode) override;
    LoadStatus loadInteractive(const QString &path, AppModel *model, QString &errMsg,
                               ImportMode mode, LoadOptions &options);
    bool save(const QString &path, AppModel *model, QString &errMsg) override;

protected:
    // Present the interactive import choice. The base implementation offers no
    // UI and cancels; the app overrides it to show a dialog. `reconverter`
    // re-derives the track list when the user toggles separate-channels.
    // Returns false when the user cancels.
    virtual bool chooseImportOptions(const QList<MidiImportTrackInfo> &initialTracks,
                                     MidiTrackReconverter &reconverter, bool defaultImportTempo,
                                     bool defaultImportTimeSignature, MidiImportOptions &out) {
        Q_UNUSED(initialTracks);
        Q_UNUSED(reconverter);
        Q_UNUSED(defaultImportTempo);
        Q_UNUSED(defaultImportTimeSignature);
        Q_UNUSED(out);
        return false;
    }

    // App-provided import settings (replace direct AppOptions reads so the
    // converter itself stays free of app settings).
    virtual QString importLanguage() const {
        return {};
    }

    virtual QString defaultLyric(const QString &language) const {
        Q_UNUSED(language);
        return {};
    }
};

// Parsed MIDI payload: reusable across files and import options. Producing it
// never touches the model; the interactive UI and batch imports both start
// from it.
struct MidiParseData {
    QString path;
    // Raw file bytes, kept for separate-channels re-parsing and lyric
    // encoding detection.
    QByteArray rawData;
    opendspx::MidiIntermediateData mediate;
    QList<MidiImportTrackInfo> trackInfos;
    bool valid = false;
    QString errorMessage;
};

// Stage 1 of the split MIDI pipeline: file -> reusable intermediate data.
// No UI, no import options, no model mutation.
class MidiFileParser {
public:
    static MidiParseData parse(const QString &path);
};

// Tracks generated from parsed MIDI data, ready to be committed. Generation
// itself never mutates the model; the caller applies the timeline and inserts
// the tracks (optionally as one undoable history item).
struct MidiGenerationResult {
    QList<Track *> tracks;
    bool hasTimeline = false;
    QList<Tempo> tempos;
    QList<TimeSignature> timeSignatures;
    QString errorMessage;
};

// Stage 2 of the split MIDI pipeline: intermediate data + import options ->
// generated track objects (no UI, no model mutation).
class MidiTrackGenerator {
public:
    static MidiGenerationResult generateTracks(MidiParseData &data, const MidiImportOptions &choice,
                                               const QString &language, const QString &defaultLyric,
                                               const Timeline &timeline);
};

#endif // DS_EDITOR_LITE_MIDICONVERTER_H
