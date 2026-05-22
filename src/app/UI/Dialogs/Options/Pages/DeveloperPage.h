//
// Created by fluty on 26-5-8.
//

#ifndef DEVELOPERPAGE_H
#define DEVELOPERPAGE_H

#include "IOptionPage.h"

class SwitchButton;

class DeveloperPage : public IOptionPage {
    Q_OBJECT

public:
    explicit DeveloperPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    SwitchButton *m_swEnableDiagnostics;
    SwitchButton *m_swShowTimelineDebugInfo;
    SwitchButton *m_swShowClipDebugInfo;
    SwitchButton *m_swEnablePanelDetach;
};


#endif // DEVELOPERPAGE_H
