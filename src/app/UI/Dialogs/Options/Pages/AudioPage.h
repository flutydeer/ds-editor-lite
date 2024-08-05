//
// Created by fluty on 24-3-18.
//

#ifndef AUDIOPAGE_H
#define AUDIOPAGE_H

#include "IOptionPage.h"
#include "Modules/Audio/AudioSystem.h"

class OutputPlaybackPageWidget;

class AudioPage : public IOptionPage {
    Q_OBJECT

public:
    explicit AudioPage(QWidget *parent = nullptr);
    ~AudioPage() override;

protected:
    void modifyOption() override;

private:
    OutputPlaybackPageWidget *m_widget;
};



#endif // AUDIOPAGE_H
