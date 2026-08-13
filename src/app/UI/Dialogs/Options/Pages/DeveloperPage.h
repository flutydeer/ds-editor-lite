#ifndef DEVELOPERPAGE_H
#define DEVELOPERPAGE_H

#include "IOptionPage.h"

class SwitchButton;
class ComboBox;

class DeveloperPage : public IOptionPage {
    Q_OBJECT

public:
    explicit DeveloperPage(QWidget *parent = nullptr);

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    void syncFromOptions();

    SwitchButton *m_swEnableDiagnostics;
    SwitchButton *m_swShowLogWindow;
    SwitchButton *m_swShowTimelineDebugInfo;
    SwitchButton *m_swShowClipDebugInfo;
    SwitchButton *m_swEnablePanelDetach;
    SwitchButton *m_swEnableEmbeddedOptionsDialog;
    ComboBox *m_cbxEditorRenderBackend;
};


#endif // DEVELOPERPAGE_H
