#include "LogWindow.h"

#include <lite/Support/LogBus.h>
#include "Model/AppOptions/AppOptions.h"

#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

#include <algorithm>

namespace {
    /// Keeps the per-level foreground color when a row is selected
    class LogItemDelegate final : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

    protected:
        void initStyleOption(QStyleOptionViewItem *option,
                             const QModelIndex &index) const override {
            QStyledItemDelegate::initStyleOption(option, index);
            if (const auto foreground = index.data(Qt::ForegroundRole);
                foreground.canConvert<QBrush>())
                option->palette.setBrush(QPalette::HighlightedText,
                                         foreground.value<QBrush>());
        }
    };
}

LogWindowModel::LogWindowModel(QObject *parent) : QAbstractTableModel(parent) {
}

int LogWindowModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_messages.size());
}

int LogWindowModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LogWindowModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() >= m_messages.size())
        return {};

    const auto &message = m_messages.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case TimeColumn:
                return message.time;
            case LevelColumn:
                return Log::LogMessage::levelText(message.level);
            case TagColumn:
                return message.tag;
            case TextColumn:
                return message.text;
            default:
                return {};
        }
    }
    if (role == Qt::ForegroundRole) {
        switch (message.level) {
            case Log::Debug:
                return QColor(0x69, 0xD1, 0xB8);
            case Log::Info:
                return QColor(0x9B, 0xBA, 0xFF);
            case Log::Warning:
                return QColor(0xDA, 0xB6, 0x69);
            case Log::Error:
            case Log::Fatal:
                return QColor(0xF7, 0xA1, 0x99);
        }
    }
    return {};
}

QVariant LogWindowModel::headerData(const int section, const Qt::Orientation orientation,
                                    const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case TimeColumn:
            return tr("Time");
        case LevelColumn:
            return tr("Level");
        case TagColumn:
            return tr("Tag");
        case TextColumn:
            return tr("Message");
        default:
            return {};
    }
}

void LogWindowModel::appendMessage(const Log::LogMessage &message) {
    const auto row = static_cast<int>(m_messages.size());
    beginInsertRows({}, row, row);
    m_messages.append(message);
    endInsertRows();

    if (m_messages.size() > maxRows) {
        beginRemoveRows({}, 0, 0);
        m_messages.removeFirst();
        endRemoveRows();
    }

    if (!message.tag.isEmpty() && !m_knownTags.contains(message.tag)) {
        m_knownTags.insert(message.tag);
        emit tagAdded(message.tag);
    }
}

void LogWindowModel::clear() {
    beginResetModel();
    m_messages.clear();
    endResetModel();
}

const Log::LogMessage &LogWindowModel::messageAt(const int row) const {
    return m_messages.at(row);
}

QStringList LogWindowModel::knownTags() const {
    auto tags = QStringList(m_knownTags.constBegin(), m_knownTags.constEnd());
    tags.sort(Qt::CaseInsensitive);
    return tags;
}

LogFilterProxyModel::LogFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
}

void LogFilterProxyModel::setMinimumLevel(const Log::LogLevel level) {
    m_minimumLevel = level;
    invalidateRowsFilter();
}

void LogFilterProxyModel::setTagFilter(const QString &tag) {
    m_tagFilter = tag;
    invalidateRowsFilter();
}

void LogFilterProxyModel::setSearchText(const QString &text) {
    m_searchText = text;
    invalidateRowsFilter();
}

bool LogFilterProxyModel::filterAcceptsRow(const int sourceRow,
                                           const QModelIndex &sourceParent) const {
    Q_UNUSED(sourceParent)
    const auto model = static_cast<LogWindowModel *>(sourceModel());
    const auto &message = model->messageAt(sourceRow);

    if (message.level < m_minimumLevel)
        return false;
    if (!m_tagFilter.isEmpty() && message.tag != m_tagFilter)
        return false;
    if (!m_searchText.isEmpty() && !message.text.contains(m_searchText, Qt::CaseInsensitive) &&
        !message.tag.contains(m_searchText, Qt::CaseInsensitive))
        return false;
    return true;
}

LogWindow::LogWindow(QWidget *parent) : Window(parent) {
    setWindowFlags(Qt::Window);

    m_model = new LogWindowModel(this);
    m_proxyModel = new LogFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    m_cbLevel = new QComboBox;
    for (const auto level : {Log::Debug, Log::Info, Log::Warning, Log::Error, Log::Fatal})
        m_cbLevel->addItem({}, level);
    connect(m_cbLevel, &QComboBox::currentIndexChanged, this, [this](const int index) {
        m_proxyModel->setMinimumLevel(static_cast<Log::LogLevel>(m_cbLevel->itemData(index).toInt()));
    });

    m_cbTag = new QComboBox;
    m_cbTag->addItem({}); // "All tags"
    m_cbTag->setMinimumWidth(160);
    connect(m_cbTag, &QComboBox::currentIndexChanged, this, [this](const int index) {
        m_proxyModel->setTagFilter(index <= 0 ? QString() : m_cbTag->currentText());
    });

    m_leSearch = new QLineEdit;
    m_leSearch->setClearButtonEnabled(true);
    connect(m_leSearch, &QLineEdit::textChanged, m_proxyModel,
            &LogFilterProxyModel::setSearchText);

    m_btnClear = new QPushButton;
    connect(m_btnClear, &QPushButton::clicked, m_model, &LogWindowModel::clear);

    m_chkAutoScroll = new QCheckBox;
    m_chkAutoScroll->setChecked(true);
    connect(m_chkAutoScroll, &QCheckBox::toggled, this, [this](const bool checked) {
        m_followTail = checked;
        scrollToBottomIfFollowing();
    });

    m_tableView = new QTableView;
    m_tableView->setModel(m_proxyModel);
    m_tableView->setItemDelegate(new LogItemDelegate(m_tableView));
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setShowGrid(false);
    m_tableView->setWordWrap(false);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setDefaultSectionSize(22);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    // Left-align header text to line up with column contents
    m_tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // Do not bold the header of the selected column
    m_tableView->horizontalHeader()->setHighlightSections(false);
    // Right-click menu: copy selected rows / message text only
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                if (!m_tableView->selectionModel()->hasSelection())
                    return;
                QMenu menu(m_tableView);
                menu.addAction(tr("Copy"), QKeySequence::Copy, this, &LogWindow::copySelectedRows);
                menu.addAction(tr("Copy Message Only"), this, &LogWindow::copySelectedMessages);
                menu.exec(m_tableView->viewport()->mapToGlobal(pos));
            });
    m_tableView->setColumnWidth(LogWindowModel::TimeColumn, 100);
    m_tableView->setColumnWidth(LogWindowModel::LevelColumn, 48);
    m_tableView->setColumnWidth(LogWindowModel::TagColumn, 180);

    // Monospaced log font with CJK-capable fallback
    QFont logFont;
    logFont.setFamilies({QStringLiteral("Cascadia Code"), QStringLiteral("Microsoft YaHei UI")});
    m_tableView->setFont(logFont);
    m_tableView->setObjectName("logTableView");

    // Scrolling away from the bottom pauses following; reaching the bottom resumes it.
    // The checkbox mirrors the state and can toggle it explicitly.
    connect(m_tableView->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](const int value) {
                m_chkAutoScroll->setChecked(value >= m_tableView->verticalScrollBar()->maximum());
            });

    const auto copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &LogWindow::copySelectedRows);

    const auto toolbarLayout = new QHBoxLayout;
    toolbarLayout->addWidget(m_cbLevel);
    toolbarLayout->addWidget(m_cbTag);
    toolbarLayout->addWidget(m_leSearch, 1);
    toolbarLayout->addWidget(m_chkAutoScroll);
    toolbarLayout->addWidget(m_btnClear);

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(m_tableView);
    setLayout(mainLayout);
    resize(1280, 640);

    retranslateUi();

    // Backfill history, then follow live messages (queued: messages come from any thread)
    for (const auto &message : LogBus::instance()->recentMessages())
        onMessageLogged(message);
    connect(LogBus::instance(), &LogBus::messageLogged, this, &LogWindow::onMessageLogged,
            Qt::QueuedConnection);
    connect(m_model, &LogWindowModel::tagAdded, this, &LogWindow::onTagAdded);
    for (const auto &tag : m_model->knownTags())
        m_cbTag->addItem(tag);
}

void LogWindow::changeEvent(QEvent *event) {
    Window::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void LogWindow::showEvent(QShowEvent *event) {
    Window::showEvent(event);
    scrollToBottomIfFollowing();
}

void LogWindow::closeEvent(QCloseEvent *event) {
    // Only a user-initiated close (clicking the close button) turns the developer
    // option off. During app shutdown Qt closes child windows programmatically
    // (non-spontaneous); the option must survive that so the window reopens on restart.
    if (event->spontaneous()) {
        const auto option = appOptions->developer();
        if (option->showLogWindow) {
            option->showLogWindow = false;
            appOptions->saveAndNotify(AppOptionsGlobal::DeveloperOptions);
        }
    }
    Window::closeEvent(event);
}

void LogWindow::retranslateUi() {
    setWindowTitle(tr("Log - %1").arg(QGuiApplication::applicationDisplayName()));
    const QStringList levelNames{tr("Debug"), tr("Info"), tr("Warning"), tr("Error"), tr("Fatal")};
    for (int i = 0; i < levelNames.size() && i < m_cbLevel->count(); i++)
        m_cbLevel->setItemText(i, levelNames.at(i));
    m_cbTag->setItemText(0, tr("All tags"));
    m_leSearch->setPlaceholderText(tr("Search message or tag"));
    m_chkAutoScroll->setText(tr("Auto-scroll"));
    m_btnClear->setText(tr("Clear"));
}

void LogWindow::onMessageLogged(const Log::LogMessage &message) {
    m_model->appendMessage(message);
    scrollToBottomIfFollowing();
}

void LogWindow::onTagAdded(const QString &tag) {
    // Keep tag list sorted, index 0 is "All tags"
    int insertPos = 1;
    while (insertPos < m_cbTag->count() &&
           QString::compare(m_cbTag->itemText(insertPos), tag, Qt::CaseInsensitive) < 0)
        insertPos++;
    m_cbTag->insertItem(insertPos, tag);
}

void LogWindow::copySelectedRows() const {
    auto selection = m_tableView->selectionModel()->selectedRows();
    // selectedRows() follows selection order; sort to keep the displayed order
    std::sort(selection.begin(), selection.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    QStringList lines;
    for (const auto &proxyIndex : selection) {
        const auto sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        lines.append(m_model->messageAt(sourceIndex.row()).toPlainText());
    }
    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join('\n'));
}

void LogWindow::copySelectedMessages() const {
    auto selection = m_tableView->selectionModel()->selectedRows();
    std::sort(selection.begin(), selection.end(),
              [](const QModelIndex &a, const QModelIndex &b) { return a.row() < b.row(); });
    QStringList lines;
    for (const auto &proxyIndex : selection) {
        const auto sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        lines.append(m_model->messageAt(sourceIndex.row()).text);
    }
    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join('\n'));
}

void LogWindow::scrollToBottomIfFollowing() {
    if (m_followTail && isVisible())
        m_tableView->scrollToBottom();
}
