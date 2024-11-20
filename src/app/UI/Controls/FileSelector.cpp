#include "FileSelector.h"

FileSelector::FileSelector(QWidget *parent)
    : QWidget(parent), m_lineEdit(new LineEdit(this)),
      m_browseButton(new QPushButton("Browse", this)) {
    const auto layout = new QHBoxLayout(this);
    layout->addWidget(m_lineEdit);
    layout->addWidget(m_browseButton);
    layout->setContentsMargins(0, 0, 0, 0);

    m_lineEdit->setText(QDir::currentPath());

    connect(m_browseButton, &QPushButton::clicked, this, &FileSelector::onBrowseButtonClicked);
    connect(m_lineEdit, &QLineEdit::editingFinished, this, &FileSelector::onTextChanged);
}

QString FileSelector::filePath() const {
    return m_lineEdit->text();
}

void FileSelector::onBrowseButtonClicked() {
    const QString selectedFile =
        QFileDialog::getOpenFileName(this, "Select a File", QDir::currentPath());
    if (!selectedFile.isEmpty()) {
        m_lineEdit->setText(selectedFile);
        emit filePathChanged(selectedFile);
    }
}

void FileSelector::onTextChanged() {
    emit filePathChanged(m_lineEdit->text());
}
