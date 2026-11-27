#pragma once

class QMenuBar;

namespace MacApplicationMenu {
    bool isCurrent(QMenuBar &bar);
    bool triggerCommand(char key);
    void reopen(bool hasVisibleWindows);
}
