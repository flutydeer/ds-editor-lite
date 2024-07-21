#ifndef DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H
#define DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H

#include "Modules/Audio/AudioSystem.h"
#include "UI/Dialogs/Base/OKCancelApplyDialog.h"

class OutputPlaybackPageWidget;

class AudioSettingsDialog : public OKCancelApplyDialog {
    Q_OBJECT
public:
    explicit AudioSettingsDialog(QWidget *parent = nullptr);
private:
    OutputPlaybackPageWidget *m_widget;
    void applySetting() const;
};



#endif // DS_EDITOR_LITE_AUDIOSETTINGSDIALOG_H
