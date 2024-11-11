#include "SearchDialog.h"

#include "Controller/ClipController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Note.h"

#include <QApplication>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>

SearchDialog::SearchDialog(SingingClip *singingClip, QWidget *parent)
    : Dialog(parent), m_clip(singingClip), m_notes(m_clip->notes().toList()) {
    setModal(true);
    setWindowTitle("搜索歌词");

    resize(200, 80);

    lineEditSearch = new QLineEdit();
    auto *searchLayout = new QHBoxLayout();
    btnPrev = new QPushButton("↑");
    btnNext = new QPushButton("↓");

    searchLayout->addWidget(lineEditSearch);
    searchLayout->addWidget(btnPrev);
    searchLayout->addWidget(btnNext);

    resultListWidget = new QListWidget();
    labelInfo = new QLabel("找到了 0 个匹配项");

    auto *layout = new QVBoxLayout();
    layout->addLayout(searchLayout);
    layout->addWidget(labelInfo);
    layout->addWidget(resultListWidget);

    body()->setLayout(layout);

    searchText = "请输入搜索内容";

    connect(lineEditSearch, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
    connect(resultListWidget, &QListWidget::currentRowChanged, this,
            &SearchDialog::onItemSelectionChanged);
    connect(btnPrev, &QPushButton::clicked, this, &SearchDialog::onPrevClicked);
    connect(btnNext, &QPushButton::clicked, this, &SearchDialog::onNextClicked);

    btnPrev->setEnabled(false);
    btnNext->setEnabled(false);

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
    if (resultListWidget->count() > 0)
        resultListWidget->setCurrentRow(0);
    updateButtonState();
}

void SearchDialog::onItemSelectionChanged(const int row) const {
    const QListWidgetItem *item = resultListWidget->item(row);
    if (item) {
        const int noteId = item->data(Qt::UserRole).toInt();
        const auto &note = m_clip->findNoteById(noteId);
        clipController->centerAt(note->start(), note->keyIndex());
    }
}

void SearchDialog::onPrevClicked() const {
    int currentRow = resultListWidget->currentRow();
    if (currentRow > 0) {
        currentRow--;
    } else {
        currentRow = resultListWidget->count() - 1;
    }
    resultListWidget->setCurrentRow(currentRow);
}

void SearchDialog::onNextClicked() const {
    int currentRow = resultListWidget->currentRow();
    if (currentRow < resultListWidget->count() - 1) {
        currentRow++;
    } else {
        currentRow = 0;
    }
    resultListWidget->setCurrentRow(currentRow);
}

void SearchDialog::updateButtonState() const {
    const bool hasItems = resultListWidget->count() > 0;
    btnPrev->setEnabled(hasItems);
    btnNext->setEnabled(hasItems);
}
