#include "SearchDialog.h"

#include "Controller/ClipController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Note.h"

#include <QApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QMessageBox>

SearchDialog::SearchDialog(SingingClip *singingClip, QWidget *parent)
    : Dialog(parent), m_clip(singingClip), m_notes(m_clip->notes().toList()) {
    setModal(true);
    setWindowTitle("搜索歌词");

    resize(200, 80);

    lineEditSearch = new QLineEdit();
    resultListWidget = new QListWidget();
    labelInfo = new QLabel("请输入搜索内容");

    auto *layout = new QVBoxLayout();
    layout->addWidget(labelInfo);
    layout->addWidget(lineEditSearch);
    layout->addWidget(resultListWidget);

    body()->setLayout(layout);

    searchText = "请输入搜索内容";

    connect(lineEditSearch, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
    connect(resultListWidget, &QListWidget::currentRowChanged, this,
            &SearchDialog::onItemSelectionChanged);

    if (parent) {
        const QRect parentGeometry = QApplication::primaryScreen()->geometry();
        const int parentWidth = parentGeometry.width();
        const int parentHeight = parentGeometry.height();

        move(parentGeometry.left() + parentWidth * 3 / 4 - width() / 2,
             parentGeometry.top() + parentHeight / 4 - height() / 2);
    }
}

SearchDialog::~SearchDialog() = default;

void SearchDialog::onSearchTextChanged(const QString &searchTerm) {
    resultListWidget->clear();
    labelInfo->clear();

    if (searchTerm.isEmpty()) {
        labelInfo->setText("请输入搜索内容");
        return;
    }

    for (const auto &note : m_notes) {
        if (note->lyric().startsWith(searchTerm, Qt::CaseInsensitive)) {
            QString displayText =
                QString("%1 (起始tick: %2)").arg(note->lyric(), QString::number(note->start()));
            auto *item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, note->id());
            resultListWidget->addItem(item);
        }
    }

    labelInfo->setText(QString("找到了 %1 个匹配项").arg(resultListWidget->count()));
}

void SearchDialog::onItemSelectionChanged(const int row) const {
    const QListWidgetItem *item = resultListWidget->item(row);
    if (item) {
        const int noteId = item->data(Qt::UserRole).toInt();
        const auto &note = m_clip->findNoteById(noteId);
        clipController->centerAt(note->start(), note->keyIndex());
    }
}