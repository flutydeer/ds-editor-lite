#ifndef MIDIPAGE_H
#define MIDIPAGE_H

#include "IOptionPage.h"

class MIDIPageWidget;

class MidiPage : public IOptionPage {
    Q_OBJECT
public:
    explicit MidiPage(QWidget *parent = nullptr);
    ~MidiPage() override;

protected:
    void modifyOption() override;
    QWidget *createContentWidget() override;

private:
    MIDIPageWidget *m_widget;
};



#endif // MIDIPAGE_H
