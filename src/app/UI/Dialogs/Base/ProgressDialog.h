#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include "Dialog.h"

#include "Global/TaskGlobal.h"

class Button;
class ProgressIndicator;

class ProgressDialog : public Dialog {
    Q_OBJECT

public:
    explicit ProgressDialog(bool cancellable = true, bool canHide = true,
                            QWidget *parent = nullptr);

    void forceClose();
    void setProgressRange(double minimum, double maximum) const;
    void setProgressValue(double value) const;
    void setProgressIndeterminate(bool indeterminate) const;
    void setProgressStatus(TaskGlobal::Status status) const;

signals:
    void canceled();

protected:
    void closeEvent(QCloseEvent *event) override;
    virtual void onCanceled();

private:
    bool m_canHide;
    bool m_cancellable;
    Button *m_btnHide = nullptr;
    Button *m_btnCancel = nullptr;
    ProgressIndicator *m_progressBar = nullptr;
};

#endif // PROGRESSDIALOG_H
