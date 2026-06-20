#ifndef DS_EDITOR_LITE_SPEAKERMIXDIALOG_H
#define DS_EDITOR_LITE_SPEAKERMIXDIALOG_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/PackageManager/Models/SingerInfo.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"

#include <QMap>

class TagButton;
class SpeakerMixList;
using SpeakerMixModel::SpeakerMixData;

class SpeakerMixDialog : public OKCancelDialog {
    Q_OBJECT

public:
    explicit SpeakerMixDialog(const SingerInfo &singerInfo, const SpeakerMixData &mixData,
                              QWidget *parent = nullptr);

    SpeakerMixData speakerMixData() const;

private slots:
    void onTagToggled(bool checked);
    void onSpeakerChangedInList(const QString &oldName, const QString &newName);

private:
    void setupInitialSources(const SpeakerMixData &mixData);
    void updateTagStates();
    QVector<double> currentFullWeights() const;
    SpeakerInfo speakerById(const QString &id) const;

    SpeakerMixList *m_mixList = nullptr;
    QMap<QString, TagButton *> m_tagButtons;
    SingerInfo m_singerInfo;
    SpeakerMixData m_result;
};

#endif
