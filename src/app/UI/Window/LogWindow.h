#ifndef LOGWINDOW_H
#define LOGWINDOW_H

#include "Base/Window.h"
#include <lite/Support/Log.h>

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

class QComboBox;
class QCheckBox;
class QLineEdit;
class QTableView;
class QPushButton;

/// Table model holding all messages received from LogBus (bounded, oldest dropped)
class LogWindowModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column { TimeColumn, LevelColumn, TagColumn, TextColumn, ColumnCount };

    explicit LogWindowModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;

    void appendMessage(const Log::LogMessage &message);
    void clear();
    [[nodiscard]] const Log::LogMessage &messageAt(int row) const;
    [[nodiscard]] QStringList knownTags() const;

signals:
    void tagAdded(const QString &tag);

private:
    static constexpr int maxRows = 10000;

    QList<Log::LogMessage> m_messages;
    QSet<QString> m_knownTags;
};

/// Proxy applying level / tag / text filters on top of LogWindowModel
class LogFilterProxyModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit LogFilterProxyModel(QObject *parent = nullptr);

    void setMinimumLevel(Log::LogLevel level);
    void setTagFilter(const QString &tag); // empty = all tags
    void setSearchText(const QString &text);

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow,
                                        const QModelIndex &sourceParent) const override;

private:
    Log::LogLevel m_minimumLevel = Log::Debug;
    QString m_tagFilter;
    QString m_searchText;
};

/// Standalone Logcat-style log viewer window. All messages arrive unfiltered
/// via LogBus; filtering happens in the proxy model, so changing the filter
/// re-applies to history instantly.
class LogWindow final : public Window {
    Q_OBJECT

public:
    explicit LogWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void retranslateUi();
    void onMessageLogged(const Log::LogMessage &message);
    void onTagAdded(const QString &tag);
    void copySelectedRows() const;
    void copySelectedMessages() const;
    void scrollToBottomIfFollowing();

    LogWindowModel *m_model;
    LogFilterProxyModel *m_proxyModel;
    QTableView *m_tableView;
    QComboBox *m_cbLevel;
    QComboBox *m_cbTag;
    QLineEdit *m_leSearch;
    QCheckBox *m_chkAutoScroll;
    QPushButton *m_btnClear;
    bool m_followTail = true;
};

#endif // LOGWINDOW_H
