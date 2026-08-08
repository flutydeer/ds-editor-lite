#ifndef MIDICONVERTERDIALOG_H
#define MIDICONVERTERDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"
#include <lite/ProjectConverters/MidiConverter.h> // MidiImportTrackInfo

#include <QByteArray>
#include <QList>
#include <QString>

class MidiConfigPage;

// Thin dialog shell hosting MidiConfigPage. Kept for the synchronous libs
// import path (MidiConverterUi::chooseImportOptions); interactive imports
// host MidiConfigPage inside the generic ProjectImportConfigDialog.
class MidiConverterDialog : public Dialog {
    Q_OBJECT
public:
    explicit MidiConverterDialog(QWidget *parent = nullptr) : MidiConverterDialog({}, parent) {
    }

    explicit MidiConverterDialog(const QList<MidiImportTrackInfo> &trackInfoList,
                                 QWidget *parent = nullptr);
    ~MidiConverterDialog() override;

    void setTrackInfoList(const QList<MidiImportTrackInfo> &trackInfoList);
    QList<MidiImportTrackInfo> trackInfoList() const;

    QList<int> selectedTracks() const;
    QByteArray selectedCodec() const;

    bool separateMidiChannels() const;
    bool importTempo() const;
    bool importTimeSignature() const;

    void detectCodec();

    void setSelectedCodec(const QByteArray &codec);
    void setSeparateMidiChannels(bool enabled);
    void setImportTempo(bool enabled);
    void setImportTimeSignature(bool enabled);

signals:
    void codecChanged(const QByteArray &codec);
    void selectedTracksChanged();
    void separateMidiChannelsChanged(bool enabled);
    void importTempoChanged(bool enabled);
    void importTimeSignatureChanged(bool enabled);

private:
    MidiConfigPage *m_page = nullptr;
};

#endif // MIDICONVERTERDIALOG_H
