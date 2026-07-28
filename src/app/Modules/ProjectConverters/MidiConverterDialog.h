#ifndef MIDICONVERTERDIALOG_H
#define MIDICONVERTERDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"
#include "MidiConverter.h" // MidiImportTrackInfo

#include <QByteArray>
#include <QList>
#include <QScopedPointer>
#include <QString>

class MidiConverterDialogPrivate;

class MidiConverterDialog : public Dialog {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MidiConverterDialog)
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

Q_SIGNALS:
    void codecChanged(const QByteArray &codec);
    void selectedTracksChanged();
    void separateMidiChannelsChanged(bool enabled);
    void importTempoChanged(bool enabled);
    void importTimeSignatureChanged(bool enabled);

private:
    QScopedPointer<MidiConverterDialogPrivate> d_ptr;
};

#endif // MIDICONVERTERDIALOG_H
