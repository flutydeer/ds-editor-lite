#ifndef DS_EDITOR_LITE_MIDICONVERTERUI_H
#define DS_EDITOR_LITE_MIDICONVERTERUI_H

#include <lite/ProjectConverters/MidiConverter.h>

// App-side MidiConverter: supplies the interactive import dialog and the
// AppOptions-backed import settings, so the MidiConverter domain logic stays
// free of UI and app settings.
class MidiConverterUi final : public MidiConverter {
protected:
    bool chooseImportOptions(const QList<MidiImportTrackInfo> &initialTracks,
                             MidiTrackReconverter &reconverter, bool defaultImportTempo,
                             bool defaultImportTimeSignature, MidiImportOptions &out) override;
    QString importLanguage() const override;
    QString defaultLyric(const QString &language) const override;
};

#endif // DS_EDITOR_LITE_MIDICONVERTERUI_H
