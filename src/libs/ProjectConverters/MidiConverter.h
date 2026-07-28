#ifndef DS_EDITOR_LITE_MIDICONVERTER_H
#define DS_EDITOR_LITE_MIDICONVERTER_H

#include <lite/ProjectConverters/IProjectConverter.h>

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

#endif // DS_EDITOR_LITE_MIDICONVERTER_H
