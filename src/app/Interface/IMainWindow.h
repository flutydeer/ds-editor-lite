//
// Created by fluty on 2024/7/13.
//

#ifndef IMAINWINDOW_H
#define IMAINWINDOW_H

#include "Utils/Macros.h"

LITE_INTERFACE IMainWindow {
    I_DECL(IMainWindow)
    I_METHOD(void updateWindowTitle());
    I_METHOD(void quit());
    I_METHOD(void restart());
};
// END_INTERFACE
#endif // IMAINWINDOW_H
