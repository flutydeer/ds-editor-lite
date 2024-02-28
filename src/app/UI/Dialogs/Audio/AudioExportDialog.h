#ifndef DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H
#define DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H

#include "Modules/Audio/AudioExporter.h"
#include "UI/Dialogs/Base/Dialog.h"

class LineEdit;
class QTextEdit;
class ComboBox;
class Button;
class QSlider;
class QDoubleSpinBox;
class QListWidget;
class QCheckBox;
class QRadioButton;

class AudioExportDialog : public Dialog {
    Q_OBJECT
public:
    explicit AudioExportDialog(QWidget *parent = nullptr);
    ~AudioExportDialog() override;

private:
    ComboBox *m_presetComboBox;
    Button *m_presetDeleteButton;
    LineEdit *m_fileDirectoryEdit;
    LineEdit *m_fileNameEdit;
    QPushButton *m_warningButton;
    QTextEdit *m_previewFrame;
    ComboBox *m_formatTypeComboBox;
    ComboBox *m_formatOptionComboBox;
    QSlider *m_vbrSlider;
    QDoubleSpinBox *m_formatSampleRateSpinBox;
    LineEdit *m_extensionNameEdit;
    ComboBox *m_sourceComboBox;
    QListWidget *m_sourceListWidget;
    ComboBox *m_mixingOptionComboBox;
    LineEdit *m_trackAffixEdit;
    Button *m_trackAffixTemplateButton;
    QCheckBox *m_enableMuteSoloCheckBox;
    QRadioButton *m_rangeSelectAllRadio;
    QRadioButton *m_rangeLoopIntervalRadio;

    QString m_warningText;

    AudioExporter *m_exporter;

    void applyExporterOptionToDialog();
    void applyDialogToExporterOption();
    enum FileListWarning {
        EmptyWarning = 1,
        DuplicatedWarning = 2,
        OverwritingWarning = 4,
    };
    static int checkFileListWarnings(const QStringList &list);

    void onFormModified();
    bool m_holdFormModified = false;
    void updatePresetList();
    void loadPreset(const QVariant &data);
    void updateDirtyPreset();
    void presetSaveAs();
};

#endif // DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H
