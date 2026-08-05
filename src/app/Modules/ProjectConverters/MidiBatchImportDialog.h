#ifndef MIDIBATCHIMPORTDIALOG_H
#define MIDIBATCHIMPORTDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

#include <QByteArray>
#include <QScopedPointer>

class MidiBatchImportDialogPrivate;

// Single shared options dialog for a batch of MIDI files: one encoding for
// all files plus the tempo / time-signature toggles. Track selection is not
// shown — every note-bearing track is imported automatically.
class MidiBatchImportDialog final : public Dialog {
    Q_OBJECT
    Q_DECLARE_PRIVATE(MidiBatchImportDialog)
public:
    explicit MidiBatchImportDialog(const QByteArray &defaultCodec, QWidget *parent = nullptr);
    ~MidiBatchImportDialog() override;

    QByteArray selectedCodec() const;
    bool importTempo() const;
    bool importTimeSignature() const;

private:
    QScopedPointer<MidiBatchImportDialogPrivate> d_ptr;
};

#endif // MIDIBATCHIMPORTDIALOG_H
