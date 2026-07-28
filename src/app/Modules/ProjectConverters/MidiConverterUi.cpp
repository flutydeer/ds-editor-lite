#include "MidiConverterUi.h"

#include "MidiConverterDialog.h"

#include "UI/Dialogs/Base/Dialog.h"
#include "Model/AppOptions/AppOptions.h"

#include <QDialog>

bool MidiConverterUi::chooseImportOptions(const QList<MidiImportTrackInfo> &initialTracks,
                                          MidiTrackReconverter &reconverter,
                                          const bool defaultImportTempo,
                                          const bool defaultImportTimeSignature,
                                          MidiImportOptions &out) {
    MidiConverterDialog dlg(initialTracks, Dialog::globalParent());
    dlg.setImportTempo(defaultImportTempo);
    dlg.setImportTimeSignature(defaultImportTimeSignature);
    dlg.detectCodec();

    QObject::connect(&dlg, &MidiConverterDialog::separateMidiChannelsChanged, &dlg,
                     [&](bool enabled) {
                         dlg.setTrackInfoList(reconverter.reconvert(enabled));
                         dlg.detectCodec();
                     });

    if (dlg.exec() != QDialog::Accepted)
        return false;

    out.codec = dlg.selectedCodec();
    out.selectedTrackIndices = dlg.selectedTracks();
    out.importTempo = dlg.importTempo();
    out.importTimeSignature = dlg.importTimeSignature();
    return true;
}

QString MidiConverterUi::importLanguage() const {
    return appOptions->general()->defaultSingingLanguage;
}

QString MidiConverterUi::defaultLyric(const QString &language) const {
    return appOptions->general()->defaultLyricForLanguage(language);
}
