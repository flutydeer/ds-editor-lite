#include "PathEditor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QPushButton>
#include <QItemDelegate>
#include <QLineEdit>

#include "PathListWidget.h"

namespace {
    QListWidgetItem *createEditableItem(const QString &text = QString()) {
        auto item = new QListWidgetItem(text);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        return item;
    }

    bool moveSelectedItems(QListWidget *listWidget, bool up) {
        Q_ASSERT(listWidget != nullptr);

        auto selected = listWidget->selectedItems();
        if (selected.isEmpty())
            return false;

        QList<int> rows;
        rows.reserve(selected.size());
        for (auto item : selected) {
            rows.push_back(listWidget->row(item));
        }

        if (up) {
            std::sort(rows.begin(), rows.end());
        } else {
            std::sort(rows.begin(), rows.end(), std::greater<>());
        }

        int step = up ? -1 : 1;
        bool moved = false;

        for (int r : rows) {
            int targetRow = r + step;

            if (targetRow < 0 || targetRow >= listWidget->count()) {
                continue;
            }

            auto targetItem = listWidget->item(targetRow);

            if (!selected.contains(targetItem)) {
                auto item = listWidget->takeItem(r);
                listWidget->insertItem(targetRow, item);
                listWidget->setCurrentItem(item);
                moved = true;
            }
        }

        if (moved) {
            for (auto item : selected) {
                item->setSelected(true);
            }
        }
        return moved;
    }

    class ScopedUpdatesDisabler {
    public:
        inline explicit ScopedUpdatesDisabler(QWidget *widget) : m_widget(widget) {
            m_widget->setUpdatesEnabled(false);
        }

        inline ~ScopedUpdatesDisabler() {
            m_widget->setUpdatesEnabled(true);
        }

    private:
        QWidget *m_widget;
    };
}

// ---------------------- PathEditor ----------------------
PathEditor::PathEditor(QWidget *parent)
    : QWidget(parent), m_listWidget(new PathListWidget(this)),
      m_addButton(new QPushButton(tr("&Add"))),
      m_deleteButton(new QPushButton(tr("&Delete"))),
      m_upButton(new QPushButton(tr("Move &Up"))),
      m_downButton(new QPushButton(tr("Move D&own"))) {

    setupUI();
    connectSignals();
}

QStringList PathEditor::paths() const {
    QStringList result;
    auto rows = m_listWidget->count();
    result.reserve(rows);
    for (int i = 0; i < rows; i++) {
        result.append(m_listWidget->item(i)->text());
    }
    return result;
}

void PathEditor::setPaths(const QStringList &paths) {
    QSignalBlocker signalBlocker(m_listWidget);
    ScopedUpdatesDisabler updateGuard(m_listWidget);
    m_listWidget->clear();
    for (const auto &path : std::as_const(paths)) {
        if (path.isEmpty()) {
            continue;
        }
        m_listWidget->addItem(createEditableItem(path));
    }
}

PathListWidget *PathEditor::listWidget() const {
    return m_listWidget;
}

void PathEditor::setupUI() {
    m_listWidget->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listWidget->setDragEnabled(true);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDragDropOverwriteMode(false);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);

    int buttonWidth = 100;
    for (auto btn : {m_addButton, m_deleteButton, m_upButton, m_downButton}) {
        btn->setFixedWidth(buttonWidth);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    }

    auto buttonLayout = new QVBoxLayout;
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_upButton);
    buttonLayout->addWidget(m_downButton);
    buttonLayout->addStretch();

    auto mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_listWidget, 1);
    mainLayout->addLayout(buttonLayout);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    setLayout(mainLayout);
}

void PathEditor::connectSignals() {
    connect(m_addButton, &QPushButton::clicked, this, &PathEditor::onAddClicked);
    connect(m_deleteButton, &QPushButton::clicked, this, &PathEditor::onDeleteClicked);
    connect(m_upButton, &QPushButton::clicked, this, &PathEditor::onUpClicked);
    connect(m_downButton, &QPushButton::clicked, this, &PathEditor::onDownClicked);
    connect(m_listWidget, &QListView::doubleClicked, this, &PathEditor::onListDoubleClicked);
    connect(m_listWidget, &PathListWidget::doubleClickedEmpty, this,
            &PathEditor::onEmptyDoubleClicked);
    connect(
        m_listWidget->model(), &QAbstractItemModel::rowsMoved, this,
        [this](const QModelIndex &, int, int, const QModelIndex &, int) { Q_EMIT pathsChanged(); });
}

void PathEditor::editRowWithEmptyCheck(int row) {
    auto model = m_listWidget->model();
    QModelIndex idx = model->index(row, 0);
    m_listWidget->edit(idx);

    if (m_editConnection) {
        disconnect(m_editConnection);
    }
    m_editConnection =
        connect(m_listWidget->itemDelegate(), &QItemDelegate::closeEditor, this,
                [row, model, this](QWidget *editor, QAbstractItemDelegate::EndEditHint) {
                    const auto lineEdit = qobject_cast<QLineEdit *>(editor);
                    if (lineEdit && lineEdit->text().isEmpty()) {
                        if (row >= 0 && row < model->rowCount()) {
                            model->removeRow(row);
                        }
                    }
                    disconnect(m_editConnection);
                    Q_EMIT pathsChanged();
                });
}

void PathEditor::onAddClicked() {
    auto dir =
        QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Browse directory")));
    if (!dir.isEmpty()) {
        m_listWidget->addItem(createEditableItem(dir));
        Q_EMIT pathsChanged();
    }
}

void PathEditor::onDeleteClicked() {
    auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    ScopedUpdatesDisabler updateGuard(m_listWidget);
    for (auto item : items) {
        delete m_listWidget->takeItem(m_listWidget->row(item));
    }
    m_listWidget->clearSelection();
    Q_EMIT pathsChanged();
}

// PathEditor 调用示例
void PathEditor::onUpClicked() {
    if (m_listWidget->selectedItems().isEmpty()) {
        return;
    }
    ScopedUpdatesDisabler updateGuard(m_listWidget);
    if (moveSelectedItems(m_listWidget, true)) {
        Q_EMIT pathsChanged();
    }
}

void PathEditor::onDownClicked() {
    if (m_listWidget->selectedItems().isEmpty()) {
        return;
    }
    ScopedUpdatesDisabler updateGuard(m_listWidget);
    if (moveSelectedItems(m_listWidget, false)) {
        Q_EMIT pathsChanged();
    }
}

void PathEditor::onListDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) {
        return;
    }
    editRowWithEmptyCheck(index.row());
}

void PathEditor::onEmptyDoubleClicked(const QPoint &) {
    auto row = m_listWidget->count();
    auto item = createEditableItem();
    m_listWidget->insertItem(row, item);
    item->setSelected(true);
    editRowWithEmptyCheck(row);
}