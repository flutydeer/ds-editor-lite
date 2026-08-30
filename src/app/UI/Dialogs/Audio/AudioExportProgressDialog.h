#ifndef AUDIO_AUDIOEXPORTPROGRESSDIALOG_H
#define AUDIO_AUDIOEXPORTPROGRESSDIALOG_H

#include "UI/Dialogs/Base/Dialog.h"

#include <QHash>
#include <QStringList>

class AccentButton;
class Button;
class QLabel;
class QListWidget;
class ProgressIndicator;
class QCloseEvent;
class ToolButton;

namespace Audio {
    class AudioExporter;
}

namespace Audio::Internal {

    class AudioExportProgressDialog : public Dialog {
        Q_OBJECT

    public:
        explicit AudioExportProgressDialog(AudioExporter *exporter, QWidget *parent = nullptr);

        [[nodiscard]] bool isTerminal() const;
        void showSuccess(const QStringList &files);
        void showFailure(const QString &errorString);
        void dismissAfterCancellation();

    public slots:
        void reject() override;

    signals:
        void cancelRequested();
        void openFolderRequested(const QStringList &files);
        void dismissed();

    protected:
        void closeEvent(QCloseEvent *event) override;

    private:
        enum class UiState { Running, Succeeded, Failed, Dismissed };

        UiState m_uiState = UiState::Running;
        QLabel *m_phaseLabel;
        ToolButton *m_warningButton;
        QListWidget *m_warningList;
        ProgressIndicator *m_progressIndicator;
        AccentButton *m_openFolderButton;
        Button *m_closeButton;
        Button *m_cancelButton;
        QStringList m_exportedFiles;
        QHash<int, double> m_sourceProgress;
        int m_sourceCount;
        bool m_isProgressing = false;
        bool m_cancelPending = false;

        void appendWarning(const QString &message);
        void enterTerminalState(UiState state);
        void requestCancel();
        void dismissTerminal();
    };

}

#endif // AUDIO_AUDIOEXPORTPROGRESSDIALOG_H
