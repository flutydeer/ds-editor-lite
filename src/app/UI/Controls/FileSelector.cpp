#include "FileSelector.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "LineEdit.h"

FileSelector::FileSelector(QWidget *parent)
    : QWidget(parent), m_lineEdit(new LineEdit(this)),
      m_browseButton(new QPushButton(tr("Browse"), this)) {
    const auto layout = new QHBoxLayout(this);
    layout->addWidget(m_lineEdit);
    layout->addWidget(m_browseButton);
    layout->setContentsMargins(0, 0, 0, 0);

    m_lineEdit->setText(QDir::currentPath());

    connect(m_browseButton, &QPushButton::clicked, this, &FileSelector::onBrowseButtonClicked);
    connect(m_lineEdit, &QLineEdit::editingFinished, this, &FileSelector::onTextChanged);

    setAcceptDrops(true);
    setTabOrder(m_lineEdit, m_browseButton);
}

QString FileSelector::filePath() const {
    return m_lineEdit->text();
}

void FileSelector::setFilePath(const QString &filePath) {
    m_lineEdit->setText(filePath);
}

static inline bool getFileNameFromMimeData(const QMimeData *mimeData,
                                           const QSet<QString> &extensions,
                                           QString *outFilePath) {
    if (!mimeData || !mimeData->hasUrls()) {
        return false;
    }
    const auto &urls = mimeData->urls();
    if (urls.isEmpty()) {
        return false;
    }
    if (extensions.isEmpty()) {
        if (outFilePath) {
            *outFilePath = urls.first().toLocalFile();
        }
        return true;
    }
    for (const auto &url : std::as_const(urls)) {
        QString filePath = url.toLocalFile();
        if (const QFileInfo info(filePath);
            !(extensions.contains(info.suffix().toLower()) ||
              extensions.contains(info.completeSuffix().toLower()))) {
            continue;
        }
        if (outFilePath) {
            *outFilePath = std::move(filePath);
        }
        return true;
    }
    return false;
}

void FileSelector::dragEnterEvent(QDragEnterEvent *event) {
    if (getFileNameFromMimeData(event->mimeData(), m_fileDropExtensions, nullptr)) {
        event->acceptProposedAction();
    }
}

void FileSelector::dropEvent(QDropEvent *event) {
    QString filePath;
    if (getFileNameFromMimeData(event->mimeData(), m_fileDropExtensions, &filePath)) {
        event->acceptProposedAction();
        m_lineEdit->setText(filePath);
        Q_EMIT filePathChanged(filePath);
    }
}

void FileSelector::onBrowseButtonClicked() {
    const QString selectedFile =
        QFileDialog::getOpenFileName(this, tr("Select a File"), QDir::currentPath(), m_filter);
    if (!selectedFile.isEmpty()) {
        m_lineEdit->setText(selectedFile);
        Q_EMIT filePathChanged(selectedFile);
    }
}

void FileSelector::onTextChanged() {
    Q_EMIT filePathChanged(m_lineEdit->text());
}
