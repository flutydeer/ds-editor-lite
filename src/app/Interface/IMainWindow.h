//
// Created by fluty on 2024/7/13.
//

#ifndef IMAINWINDOW_H
#define IMAINWINDOW_H

class IMainWindow {
public:
    virtual ~IMainWindow() = default;
    virtual void updateWindowTitle() = 0;
    [[nodiscard]] virtual bool askSaveChanges() = 0;
    virtual void quit() = 0;
    virtual void restart() = 0;
    virtual void setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed) = 0;
};



#endif // IMAINWINDOW_H
