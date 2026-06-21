#ifndef DS_EDITOR_LITE_SPEAKERMIXDIALOG_H
#define DS_EDITOR_LITE_SPEAKERMIXDIALOG_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/PackageManager/Models/SingerInfo.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"

#include <QMap>

class Button;
class ComboBox;
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
    void onPresetSelected(int index);
    void onNewPreset();
    void onSavePreset();
    void onSaveAsPreset();
    void onDeletePreset();
    void onResetPreset();

private:
    QWidget *buildPresetBar();
    void setupInitialSources(const SpeakerMixData &mixData);
    void applySpeakerMixDataToUi(const SpeakerMixData &mixData);
    void reloadPresetCombo(const QString &selectedId = {});
    bool applyPresetToUi(const QString &presetId, bool showWarning);
    SpeakerMixData buildCurrentSpeakerMixData() const;
    SpeakerMixData equalWeightMixData() const;
    void updatePresetButtons() const;
    void updateTagStates();
    QVector<double> currentFullWeights() const;
    SpeakerInfo speakerById(const QString &id) const;

    SpeakerMixList *m_mixList = nullptr;
    QMap<QString, TagButton *> m_tagButtons;
    SingerInfo m_singerInfo;
    SpeakerMixData m_initialData;
    SpeakerMixData m_result;
    ComboBox *m_cbPresets = nullptr;
    Button *m_btnNew = nullptr;
    Button *m_btnSave = nullptr;
    Button *m_btnSaveAs = nullptr;
    Button *m_btnDelete = nullptr;
    Button *m_btnReset = nullptr;
    QString m_currentPresetId;
    bool m_isNewPreset = false;
};

#endif
