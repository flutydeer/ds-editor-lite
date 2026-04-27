#ifndef MIDICONVERTERDIALOG_H
#define MIDICONVERTERDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

#include <opendspxconverter/midi/midiintermediatedata.h>

class QTextCodec;

class MidiConverterDialogPrivate;

class MidiConverterDialog : public Dialog {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MidiConverterDialog)
public:
    explicit MidiConverterDialog(QWidget *parent = nullptr) : MidiConverterDialog({}, parent) {
    }

    explicit MidiConverterDialog(const QList<opendspx::MidiIntermediateData::Track> &trackInfoList,
                                 QWidget *parent = nullptr);
    ~MidiConverterDialog() override;

    void setTrackInfoList(const QList<opendspx::MidiIntermediateData::Track> &trackInfoList);
    QList<opendspx::MidiIntermediateData::Track> trackInfoList() const;

    QList<int> selectedTracks() const;
    QTextCodec *selectedCodec() const;
    bool useMidiTimeline() const;

private:
    QScopedPointer<MidiConverterDialogPrivate> d_ptr;
};



#endif // MIDICONVERTERDIALOG_H
