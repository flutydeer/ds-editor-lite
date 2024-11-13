//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTPITCHPARAMDIALOG_H
#define EXTRACTPITCHPARAMDIALOG_H

#include "UI/Dialogs/Base/OKCancelDialog.h"

class QListWidget;
class AudioClip;

class ExtractPitchParamDialog : public OKCancelDialog {
    Q_OBJECT

public:
    explicit ExtractPitchParamDialog(const QList<AudioClip *> &clips);
    int selectedClipId = -1;

private slots:
    void onSelectionChanged(int row);

private:
    QListWidget *clipList;
};



#endif // EXTRACTPITCHPARAMDIALOG_H
