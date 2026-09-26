#include "SearchDialog.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/ClipController.h"
#include "Controller/EditorViewController.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/GUI/Controls/SmoothScroller.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

SearchDialog::SearchDialog(SingingClip *singingClip, QWidget *parent)
    : Dialog(parent), m_clip(singingClip) {
    setModal(true);
    setWindowTitle(tr("Search Lyrics"));
    resize(150, 300);

    lineEditSearch = new QLineEdit();

    resultListWidget = new QListWidget();
    resultListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    {
        // Animate mouse-wheel scrollbar movement with OutCubic; touchpad passes through.
        // Modifier combinations (Shift/Ctrl/...) are left untouched.
        auto *smoothScroller = new SmoothScroller(this);
        smoothScroller->attachTo(resultListWidget);
    }
    labelInfo = new QLabel(tr("Found %L1 matches").arg(0));

    startWithRadioButton = new QRadioButton(tr("Starts With"));
    startWithRadioButton->setToolTip(tr("Search from the beginning of the input text"));
    fullSearchRadioButton = new QRadioButton(tr("Exact Match"));
    fullSearchRadioButton->setToolTip(tr("Exact match"));
    fuzzySearchRadioButton = new QRadioButton(tr("Contains"));
    fuzzySearchRadioButton->setToolTip(tr("Contains the text"));

    startWithRadioButton->setChecked(true);

    auto *searchModeGroup = new QButtonGroup(this);
    searchModeGroup->addButton(startWithRadioButton);
    searchModeGroup->addButton(fullSearchRadioButton);
    searchModeGroup->addButton(fuzzySearchRadioButton);

    caseSensitiveCheckBox = new QCheckBox("Cc");
    caseSensitiveCheckBox->setToolTip(tr("Case sensitive"));
    regexCheckBox = new QCheckBox(".*");
    regexCheckBox->setToolTip(tr("Regular expression"));

    auto *searchTypeLayout = new QHBoxLayout();
    searchTypeLayout->addWidget(startWithRadioButton);
    searchTypeLayout->addWidget(fullSearchRadioButton);
    searchTypeLayout->addWidget(fuzzySearchRadioButton);

    auto *checkBoxLayout = new QHBoxLayout();
    checkBoxLayout->addWidget(lineEditSearch);
    checkBoxLayout->addWidget(caseSensitiveCheckBox);
    checkBoxLayout->addWidget(regexCheckBox);

    auto *resultLayout = new QHBoxLayout();
    btnPrev = new QPushButton(tr("Previous"));
    btnNext = new QPushButton(tr("Next"));

    resultLayout->addWidget(labelInfo);
    resultLayout->addStretch();
    resultLayout->addWidget(btnPrev);
    resultLayout->addWidget(btnNext);

    // Build layout: radio buttons + checkboxes + results + list
    auto *layout = new QVBoxLayout();
    layout->addLayout(checkBoxLayout);
    layout->addLayout(searchTypeLayout);
    layout->addLayout(resultLayout);
    layout->addWidget(resultListWidget);

    body()->setLayout(layout);

    searchText = tr("Please enter search content");

    connect(lineEditSearch, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
    connect(searchModeGroup, &QButtonGroup::buttonToggled, this,
            [this](QAbstractButton *, bool checked) {
                if (checked)
                    onSearchTextChanged();
            });
    connect(caseSensitiveCheckBox, &QCheckBox::toggled, this, &SearchDialog::onSearchTextChanged);
    connect(regexCheckBox, &QCheckBox::toggled, this, &SearchDialog::onSearchTextChanged);

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
    resultListWidget->clear();
    labelInfo->clear();

    const auto mode = startWithRadioButton->isChecked()    ? QStringLiteral("starts_with")
                      : fullSearchRadioButton->isChecked() ? QStringLiteral("exact")
                                                           : QStringLiteral("contains");
    if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
        const auto matches = runtime->notes().searchNotes(
            runtime->documentVersion().documentId, Automation::ClipId(m_clip->id()),
            lineEditSearch->text(), mode, caseSensitiveCheckBox->isChecked(),
            regexCheckBox->isChecked());
        if (matches) {
            for (const auto &match : matches.get()) {
                const auto displayText = QStringLiteral("%1 (%2)").arg(
                    match.lyric, appModel->getBarBeatTickTime(m_clip->start() + match.localStart));
                auto *item = new QListWidgetItem(displayText);
                item->setData(Qt::UserRole, match.noteId.value());
                resultListWidget->addItem(item);
            }
        }
    }

    labelInfo->setText(tr("Found %L1 matches").arg(resultListWidget->count()));
    if (resultListWidget->count() > 0)
        resultListWidget->setCurrentRow(0);
    updateButtonState();
}

void SearchDialog::onItemSelectionChanged(const int row) const {
    const QListWidgetItem *item = resultListWidget->item(row);
    if (item) {
        const int noteId = item->data(Qt::UserRole).toInt();
        const auto &note = m_clip->findNoteById(noteId);
        editorViewController->showBottomPanelPage(QStringLiteral("ClipEditor"));
        editorViewController->centerPianoRollAt(note->globalStart(), note->keyIndex());
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
