#include "SearchDialog.h"

#include "Controller/ClipController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Note.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>

SearchDialog::SearchDialog(SingingClip *singingClip, QWidget *parent)
    : Dialog(parent), m_clip(singingClip), m_notes(m_clip->notes().toList()) {
    setModal(true);
    setWindowTitle("搜索歌词");

    resize(150, 300);

    lineEditSearch = new QLineEdit();

    resultListWidget = new QListWidget();
    resultListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    labelInfo = new QLabel("找到了 0 个匹配项");

    // 新建单选按钮控件
    startWithRadioButton = new QRadioButton("起始匹配");
    startWithRadioButton->setToolTip("以输入文本起始搜索");
    fullSearchRadioButton = new QRadioButton("完全匹配");
    startWithRadioButton->setToolTip("完全匹配");
    fuzzySearchRadioButton = new QRadioButton("包含搜索");
    fuzzySearchRadioButton->setToolTip("包含文本");

    startWithRadioButton->setChecked(true);

    auto *searchModeGroup = new QButtonGroup(this);
    searchModeGroup->addButton(startWithRadioButton);
    searchModeGroup->addButton(fullSearchRadioButton);
    searchModeGroup->addButton(fuzzySearchRadioButton);

    // 新建复选框控件
    caseSensitiveCheckBox = new QCheckBox("区分大小写");
    regexCheckBox = new QCheckBox("正则表达式");

    // 布局调整
    auto *searchTypeLayout = new QHBoxLayout();
    searchTypeLayout->addWidget(startWithRadioButton);
    searchTypeLayout->addWidget(fullSearchRadioButton);
    searchTypeLayout->addWidget(fuzzySearchRadioButton);

    auto *checkBoxLayout = new QHBoxLayout();
    checkBoxLayout->addWidget(caseSensitiveCheckBox);
    checkBoxLayout->addWidget(regexCheckBox);

    auto *resultLayout = new QHBoxLayout();
    btnPrev = new QPushButton("上一个");
    btnNext = new QPushButton("下一个");

    resultLayout->addWidget(labelInfo);
    resultLayout->addStretch();
    resultLayout->addWidget(btnPrev);
    resultLayout->addWidget(btnNext);

    // 修改布局，将单选按钮布局和复选框布局添加到主布局中
    auto *layout = new QVBoxLayout();
    layout->addWidget(lineEditSearch);
    layout->addLayout(checkBoxLayout);
    layout->addLayout(searchTypeLayout);
    layout->addLayout(resultLayout);
    layout->addWidget(resultListWidget);

    body()->setLayout(layout);

    searchText = "请输入搜索内容";

    connect(lineEditSearch, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
    connect(startWithRadioButton, &QRadioButton::toggled, this, &SearchDialog::onSearchTextChanged);
    connect(fuzzySearchRadioButton, &QRadioButton::toggled, this,
            &SearchDialog::onSearchTextChanged);
    connect(caseSensitiveCheckBox, &QCheckBox::toggled, this, &SearchDialog::onSearchTextChanged);
    connect(regexCheckBox, &QCheckBox::toggled, this, &SearchDialog::onSearchTextChanged);
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

void SearchDialog::onSearchTextChanged() {
    const QString &searchTerm = lineEditSearch->text();
    resultListWidget->clear();
    labelInfo->clear();

    const Qt::CaseSensitivity caseSensitivity =
        caseSensitiveCheckBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const bool useRegex = regexCheckBox->isChecked();
    const bool useStart = startWithRadioButton->isChecked();
    const bool useFull = fullSearchRadioButton->isChecked();

    for (const auto &note : m_notes) {
        QString lyric = note->lyric();

        bool match;
        if (useStart) {
            if (useRegex) {
                QRegularExpression regex(
                    "^" + searchTerm,
                    static_cast<QRegularExpression::PatternOption>(!caseSensitivity));
                match = lyric.contains(regex);
            } else {
                match = lyric.startsWith(searchTerm, caseSensitivity);
            }
        } else if (useFull) {
            if (useRegex) {
                QRegularExpression regex(
                    "^" + searchTerm + "$",
                    static_cast<QRegularExpression::PatternOption>(!caseSensitivity));
                match = lyric.contains(regex);
            } else {
                match = (lyric.compare(searchTerm, caseSensitivity) == 0);
            }
        } else {
            if (useRegex) {
                QRegularExpression regex(
                    searchTerm, static_cast<QRegularExpression::PatternOption>(!caseSensitivity));
                match = lyric.contains(regex);
            } else {
                match = lyric.contains(searchTerm, caseSensitivity);
            }
        }


        if (match) {
            QString displayText =
                QString("%1 (%2)")
                    .arg(note->lyric(), appModel->getBarBeatTickTime(note->globalStart()));
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
        clipController->centerAt(note->globalStart(), note->keyIndex());
        clipController->selectNotes({note->id()}, true);
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
