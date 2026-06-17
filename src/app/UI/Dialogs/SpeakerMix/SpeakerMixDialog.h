#ifndef DS_EDITOR_LITE_SPEAKERMIXDIALOG_H
#define DS_EDITOR_LITE_SPEAKERMIXDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

#include <QMap>

class TagButton;
class SpeakerMixList;

class SpeakerMixDialog : public Dialog {
    Q_OBJECT

public:
    explicit SpeakerMixDialog(QWidget *parent = nullptr);

private slots:
    void onTagToggled(bool checked);
    void onSpeakerChangedInList(const QString &oldName, const QString &newName);

private:
    void updateTagStates();

    SpeakerMixList *m_mixList = nullptr;
    QMap<QString, TagButton *> m_tagButtons;
};

#endif
