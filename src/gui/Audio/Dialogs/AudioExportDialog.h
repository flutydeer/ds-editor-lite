#ifndef DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H
#define DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H

#include <QDialog>

class AudioExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit AudioExportDialog(QWidget *parent = nullptr);
    ~AudioExportDialog();
};

#endif // DS_EDITOR_LITE_AUDIOEXPORTDIALOG_H
