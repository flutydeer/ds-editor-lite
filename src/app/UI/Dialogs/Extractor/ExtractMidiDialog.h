//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTMIDIDIALOG_H
#define EXTRACTMIDIDIALOG_H

#include "UI/Dialogs/Base/OKCancelDialog.h"

class QListWidget;
class AudioClip;

class ExtractMidiDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    explicit ExtractMidiDialog(const QList<AudioClip *> &clips);
    int selectedClipId = -1;

private slots:
    void onSelectionChanged(int row);

private:
    QListWidget *clipList;
};

#endif // EXTRACTMIDIDIALOG_H
