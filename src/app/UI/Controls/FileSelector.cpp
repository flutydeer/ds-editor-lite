#include "FileSelector.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMimeData>
#include <QPushButton>
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

void FileSelector::dragEnterEvent(QDragEnterEvent *event) {
    const auto mimeData = event->mimeData();
    if (mimeData && mimeData->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileSelector::dropEvent(QDropEvent *event) {
    const auto mimeData = event->mimeData();
    if (!mimeData || !mimeData->hasUrls()) {
        return;
    }
    const auto &urls = mimeData->urls();
    if (urls.isEmpty()) {
        return;
    }
    const auto filePath = urls.first().toLocalFile();
    if (filePath.isEmpty()) {
        return;
    }
    m_lineEdit->setText(filePath);
}

void FileSelector::onBrowseButtonClicked() {
    const QString selectedFile =
        QFileDialog::getOpenFileName(this, tr("Select a File"), QDir::currentPath(), m_filter);
    if (!selectedFile.isEmpty()) {
        m_lineEdit->setText(selectedFile);
        emit filePathChanged(selectedFile);
    }
}

void FileSelector::onTextChanged() {
    emit filePathChanged(m_lineEdit->text());
}
