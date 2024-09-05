//
// Created by fluty on 2024/7/13.
//

#ifndef IMAINWINDOW_H
#define IMAINWINDOW_H

#include "Utils/Macros.h"

// DECL_INTERFACE(IMainWindow)
interface IMainWindow {
    I_DECL(IMainWindow)
    I_METHOD(void updateWindowTitle());
    I_NODSCD(bool askSaveChanges());
    I_METHOD(void quit());
    I_METHOD(void restart());
    I_METHOD(void setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed));
};
// END_INTERFACE
#endif // IMAINWINDOW_H
