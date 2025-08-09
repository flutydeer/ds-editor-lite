#include "DirSelector.h"

#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "LineEdit.h"

DirSelector::DirSelector(QWidget *parent)
    : QWidget(parent), m_lineEdit(new LineEdit(this)),
      m_browseButton(new QPushButton(tr("Browse"), this)) {
    const auto layout = new QHBoxLayout(this);
    layout->addWidget(m_lineEdit);
    layout->addWidget(m_browseButton);
    layout->setContentsMargins(0, 0, 0, 0);

    m_lineEdit->setText(QDir::currentPath());

    connect(m_browseButton, &QPushButton::clicked, this, &DirSelector::onBrowseButtonClicked);
    connect(m_lineEdit, &QLineEdit::editingFinished, this, &DirSelector::onTextChanged);

    setAcceptDrops(true);
    setTabOrder(m_lineEdit, m_browseButton);
}

QString DirSelector::path() const {
    return m_lineEdit->text();
}

void DirSelector::setPath(const QString &path) const {
    m_lineEdit->setText(path);
}

static bool getPathFromMimeData(const QMimeData *mimeData, QString *outPath) {
    if (!mimeData || !mimeData->hasUrls()) {
        return false;
    }
    const auto &urls = mimeData->urls();
    if (urls.isEmpty()) {
        return false;
    }
    for (const auto &url : urls) {
        QString path = url.toLocalFile();
        QFileInfo info(path);
        if (info.exists() && info.isDir()) {
            if (outPath) {
                *outPath = std::move(path);
            }
            return true;
        }
    }
    return false;
}

void DirSelector::dragEnterEvent(QDragEnterEvent *event) {
    if (getPathFromMimeData(event->mimeData(), nullptr)) {
        event->acceptProposedAction();
    }
}

void DirSelector::dropEvent(QDropEvent *event) {
    QString dirPath;
    if (getPathFromMimeData(event->mimeData(), &dirPath)) {
        event->acceptProposedAction();
        m_lineEdit->setText(dirPath);
        Q_EMIT pathChanged(dirPath);
    }
}

void DirSelector::onBrowseButtonClicked() {
    const QString selectedDir =
        QFileDialog::getExistingDirectory(this, tr("Select a Directory"), QDir::currentPath());
    if (!selectedDir.isEmpty()) {
        m_lineEdit->setText(selectedDir);
        Q_EMIT pathChanged(selectedDir);
    }
}

void DirSelector::onTextChanged() {
    Q_EMIT pathChanged(m_lineEdit->text());
}
