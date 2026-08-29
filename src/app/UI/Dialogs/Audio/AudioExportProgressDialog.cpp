#include "AudioExportProgressDialog.h"

#include "Modules/Audio/AudioExporter.h"
#include "Modules/Audio/AudioExporter_p.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ProgressIndicator.h>
#include <lite/GUI/Controls/ToolButton.h>

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <utility>

namespace Audio::Internal {

    AudioExportProgressDialog::AudioExportProgressDialog(AudioExporter *exporter, QWidget *parent)
        : Dialog(parent), m_sourceCount(qMax(1, exporter->config().source().size())) {
        setWindowTitle(tr("Export Audio"));
        setTitle(tr("Export Audio"));
        setMinimumWidth(420);
        setWindowModality(Qt::ApplicationModal);

        const auto mainLayout = new QVBoxLayout(body());
        mainLayout->setContentsMargins({});
        mainLayout->setSpacing(12);

        const auto phaseLayout = new QHBoxLayout;
        m_phaseLabel = new QLabel(tr("Preparing..."), this);
        m_phaseLabel->setWordWrap(true);
        phaseLayout->addWidget(m_phaseLabel, 1);

        m_warningButton = new ToolButton(this);
        m_warningButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
        m_warningButton->setCheckable(true);
        m_warningButton->setVisible(false);
        phaseLayout->addWidget(m_warningButton);
        mainLayout->addLayout(phaseLayout);

        m_progressIndicator = new ProgressIndicator(ProgressIndicator::HorizontalBar, this);
        m_progressIndicator->setRange(0, 100);
        m_progressIndicator->setValue(0);
        mainLayout->addWidget(m_progressIndicator);

        m_warningList = new QListWidget(this);
        m_warningList->setSelectionMode(QAbstractItemView::NoSelection);
        m_warningList->setWordWrap(true);
        m_warningList->setVisible(false);
        m_warningList->setMinimumHeight(120);
        mainLayout->addWidget(m_warningList);

        m_openFolderButton =
            new AccentButton(QIcon(QStringLiteral(":/svg/icons/folder_open_16_regular.svg")),
                             tr("Open Folder"), this);
        m_openFolderButton->setVisible(false);
        m_openFolderButton->setEnabled(false);
        m_closeButton = new Button(tr("Close"), this);
        m_closeButton->setVisible(false);
        m_closeButton->setEnabled(false);
        m_cancelButton = new Button(tr("Cancel"), this);
        m_cancelButton->setDefault(true);
        buttonBar()->addButton(m_openFolderButton);
        buttonBar()->addButton(m_closeButton);
        buttonBar()->addButton(m_cancelButton);

        connect(m_warningButton, &QAbstractButton::toggled, m_warningList, &QWidget::setVisible);
        connect(m_cancelButton, &QAbstractButton::clicked, this,
                &AudioExportProgressDialog::requestCancel);
        connect(m_closeButton, &QAbstractButton::clicked, this,
                &AudioExportProgressDialog::dismissTerminal);
        connect(m_openFolderButton, &QAbstractButton::clicked, this,
                [this] { emit openFolderRequested(m_exportedFiles); });

        connect(exporter, &AudioExporter::progressChanged, this,
                [this, exporter](const double ratio, const int sourceIndex) {
                    if (!m_isProgressing) {
                        m_isProgressing = true;
                        m_phaseLabel->setText(tr("Exporting..."));
                    }
                    if (exporter->config().mixingOption() == AudioExporterConfig::MO_Mixed ||
                        sourceIndex < 0) {
                        m_progressIndicator->setValue(ratio * 100.0);
                        return;
                    }
                    m_sourceProgress[sourceIndex] = ratio;
                    double totalRatio = 0;
                    for (const auto value : std::as_const(m_sourceProgress))
                        totalRatio += value;
                    m_progressIndicator->setValue(totalRatio / m_sourceCount * 100.0);
                });
        connect(exporter, &AudioExporter::inferenceProgressChanged, this,
                [this](const double ratio) {
                    if (m_isProgressing)
                        return;
                    m_phaseLabel->setText(tr("Running inference..."));
                    m_progressIndicator->setValue(ratio * 100.0);
                });
        connect(exporter, &AudioExporter::clippingDetected, this,
                [this, exporter](const int sourceIndex) {
                    if (sourceIndex < 0) {
                        appendWarning(tr("Clipping is detected"));
                    } else {
                        appendWarning(tr("Clipping is detected in track %L1 \"%2\"")
                                          .arg(sourceIndex + 1)
                                          .arg(exporter->d_func()->trackName(sourceIndex)));
                    }
                });
        connect(exporter, &AudioExporter::warningAdded, this,
                [this](const QString &message, const int sourceIndex) {
                    Q_UNUSED(sourceIndex);
                    appendWarning(message);
                });
    }

    bool AudioExportProgressDialog::isTerminal() const {
        return m_uiState == UiState::Succeeded || m_uiState == UiState::Failed;
    }

    void AudioExportProgressDialog::showSuccess(const QStringList &files) {
        m_exportedFiles = files;
        m_progressIndicator->setValue(100);
        if (m_warningList->count()) {
            m_progressIndicator->setTaskStatus(TaskGlobal::Warning);
            m_phaseLabel->setText(
                tr("Export finished with %Ln warning(s)", nullptr, m_warningList->count()));
            QApplication::beep();
        } else {
            m_phaseLabel->setText(tr("Export finished"));
        }
        setWindowTitle(tr("Export finished"));
        setTitle(tr("Export finished"));
        enterTerminalState(UiState::Succeeded);
    }

    void AudioExportProgressDialog::showFailure(const QString &errorString) {
        m_progressIndicator->setTaskStatus(TaskGlobal::Error);
        m_phaseLabel->setText(errorString.isEmpty() ? tr("Export failed")
                                                    : tr("Export failed\n%1").arg(errorString));
        setWindowTitle(tr("Export failed"));
        setTitle(tr("Export failed"));
        QApplication::beep();
        enterTerminalState(UiState::Failed);
    }

    void AudioExportProgressDialog::dismissAfterCancellation() {
        m_uiState = UiState::Dismissed;
        hide();
        deleteLater();
    }

    void AudioExportProgressDialog::reject() {
        if (m_uiState == UiState::Running) {
            requestCancel();
            return;
        }
        if (isTerminal())
            dismissTerminal();
    }

    void AudioExportProgressDialog::closeEvent(QCloseEvent *event) {
        if (m_uiState == UiState::Running) {
            requestCancel();
            event->ignore();
            return;
        }
        if (isTerminal()) {
            event->ignore();
            dismissTerminal();
            return;
        }
        Dialog::closeEvent(event);
    }

    void AudioExportProgressDialog::appendWarning(const QString &message) {
        const auto item =
            new QListWidgetItem(style()->standardIcon(QStyle::SP_MessageBoxWarning), message);
        m_warningList->addItem(item);
        m_warningButton->setVisible(true);
        m_warningButton->setToolTip(tr("%Ln warning(s)", nullptr, m_warningList->count()));
        QApplication::beep();
    }

    void AudioExportProgressDialog::enterTerminalState(const UiState state) {
        hide();
        m_uiState = state;
        setWindowModality(Qt::NonModal);
        setModal(false);

        m_cancelButton->setDefault(false);
        m_cancelButton->setEnabled(false);
        m_cancelButton->setVisible(false);
        m_closeButton->setEnabled(true);
        m_closeButton->setVisible(true);
        m_openFolderButton->setVisible(state == UiState::Succeeded);
        m_openFolderButton->setEnabled(state == UiState::Succeeded);
        if (state == UiState::Succeeded) {
            m_openFolderButton->setDefault(true);
        } else {
            m_closeButton->setDefault(true);
        }

        show();
        raise();
        activateWindow();
    }

    void AudioExportProgressDialog::requestCancel() {
        if (m_uiState != UiState::Running || m_cancelPending)
            return;
        m_cancelPending = true;
        m_cancelButton->setEnabled(false);
        emit cancelRequested();
    }

    void AudioExportProgressDialog::dismissTerminal() {
        if (!isTerminal())
            return;
        m_uiState = UiState::Dismissed;
        emit dismissed();
        done(QDialog::Accepted);
    }

}
