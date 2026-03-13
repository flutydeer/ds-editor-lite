#include "FileSelector.h"

#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QDir>

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

QString FileSelector::path() const {
    return m_lineEdit->text();
}

void FileSelector::setPath(const QString &filePath) const {
    m_lineEdit->setText(filePath);
}

static bool getPathFromMimeData(const QMimeData *mimeData, const QSet<QString> &extensions,
                                QString *outPath, bool folderMode = false) {
    if (!mimeData || !mimeData->hasUrls()) {
        return false;
    }
    const auto &urls = mimeData->urls();
    if (urls.isEmpty()) {
        return false;
    }

    for (const auto &url : std::as_const(urls)) {
        QString path = url.toLocalFile();

        if (folderMode) {
            // 检查路径是否为目录
            QFileInfo info(path);
            if (info.isDir()) {
                if (outPath) {
                    *outPath = path;
                }
                return true;
            }
        } else {
            // 原来的文件处理逻辑
            if (extensions.isEmpty()) {
                if (outPath) {
                    *outPath = path;
                }
                return true;
            }

            QFileInfo info(path);
            if (info.isFile() && (extensions.contains(info.suffix().toLower()) ||
                                  extensions.contains(info.completeSuffix().toLower()))) {
                if (outPath) {
                    *outPath = std::move(path);
                }
                return true;
            }
        }
    }
    return false;
}

void FileSelector::dragEnterEvent(QDragEnterEvent *event) {
    if (getPathFromMimeData(event->mimeData(), m_fileDropExtensions, nullptr, m_dirMode)) {
        event->acceptProposedAction();
    }
}

void FileSelector::dropEvent(QDropEvent *event) {
    QString filePath;
    if (getPathFromMimeData(event->mimeData(), m_fileDropExtensions, &filePath, m_dirMode)) {
        event->acceptProposedAction();
        m_lineEdit->setText(filePath);
        Q_EMIT pathChanged(filePath);
    }
}

void FileSelector::onBrowseButtonClicked() {
    QString selectedPath;
    if (m_dirMode) {
        selectedPath =
            QFileDialog::getExistingDirectory(this, tr("Select a Folder"), QDir::currentPath());
    } else {
        selectedPath =
            QFileDialog::getOpenFileName(this, tr("Select a File"), QDir::currentPath(), m_filter);
    }

    if (!selectedPath.isEmpty()) {
        m_lineEdit->setText(selectedPath);
        Q_EMIT pathChanged(selectedPath);
    }
}

void FileSelector::onTextChanged() {
    Q_EMIT pathChanged(m_lineEdit->text());
}