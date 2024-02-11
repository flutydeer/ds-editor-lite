//
// Created by fluty on 2024/2/10.
//

#include "ClipEditorViewController.h"
#include "TracksViewController.h"

void ClipEditorViewController::onClipPropertyChanged(const DsClip::ClipCommonProperties &args) {
    TracksViewController::instance()->onClipPropertyChanged(args);
}