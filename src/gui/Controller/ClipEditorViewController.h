//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEWCONTROLLER_H
#define CLIPEDITVIEWCONTROLLER_H

#include <QObject>

#include "Model/DsClip.h"
#include "Model/AppModel.h"
#include "Utils/Singleton.h"

class ClipEditorViewController final : public QObject, public Singleton<ClipEditorViewController> {
    Q_OBJECT

public slots:
    void onClipPropertyChanged(const DsClip::ClipCommonProperties &args);
};



#endif // CLIPEDITVIEWCONTROLLER_H
