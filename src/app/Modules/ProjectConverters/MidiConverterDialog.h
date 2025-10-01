#ifndef MIDICONVERTERDIALOG_H
#define MIDICONVERTERDIALOG_H

#include <QDialog>

#include <opendspx/converters/midi.h>

class QTextCodec;

class MidiConverterDialogPrivate;

class MidiConverterDialog : public QDialog {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MidiConverterDialog)
public:
    explicit inline MidiConverterDialog(QWidget *parent = nullptr)
        : MidiConverterDialog({}, parent) {
    }

    explicit MidiConverterDialog(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList,
                                 QWidget *parent = nullptr);
    ~MidiConverterDialog() override;

    void setTrackInfoList(const QList<QDspx::MidiConverter::TrackInfo> &trackInfoList);
    QList<QDspx::MidiConverter::TrackInfo> trackInfoList() const;

    QList<int> selectedTracks() const;
    QTextCodec *selectedCodec() const;
    bool useMidiTimeline() const;

private:
    QScopedPointer<MidiConverterDialogPrivate> d_ptr;
};



#endif // MIDICONVERTERDIALOG_H
