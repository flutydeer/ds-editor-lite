#ifndef DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H
#define DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H

#include "Audio/AudioExporter.h"
#include "Window/Dialogs/Dialog.h"

class QLineEdit;
class QTextEdit;
class QComboBox;
class QSlider;
class QDoubleSpinBox;
class QListWidget;
class QCheckBox;
class QRadioButton;

class AudioExportDialog : public Dialog {
    Q_OBJECT
public:
    explicit AudioExportDialog(QWidget *parent = nullptr);
    ~AudioExportDialog();

private:
    QComboBox *m_presetComboBox;
    QPushButton *m_presetDeleteButton;
    QLineEdit *m_fileDirectoryEdit;
    QLineEdit *m_fileNameEdit;
    QPushButton *m_warningButton;
    QTextEdit *m_previewFrame;
    QComboBox *m_formatTypeComboBox;
    QComboBox *m_formatOptionComboBox;
    QSlider *m_vbrSlider;
    QDoubleSpinBox *m_formatSampleRateSpinBox;
    QLineEdit *m_extensionNameEdit;
    QComboBox *m_sourceComboBox;
    QListWidget *m_sourceListWidget;
    QComboBox *m_mixingOptionComboBox;
    QLineEdit *m_trackAffixEdit;
    QPushButton *m_trackAffixTemplateButton;
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
