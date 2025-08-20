#ifndef PATHEDITOR_H
#define PATHEDITOR_H

#include <QWidget>
#include <QMetaObject>
#include <QStringList>

class PathListWidget;
class QPushButton;
class QModelIndex;
class QPoint;

class PathEditor : public QWidget {
    Q_OBJECT
public:
    explicit PathEditor(QWidget *parent = nullptr);
    ~PathEditor() override = default;

    QStringList paths() const;
    void setPaths(const QStringList &paths);

    PathListWidget *listWidget() const;
private:
    PathListWidget *m_listWidget;

    QPushButton *m_addButton;
    QPushButton *m_deleteButton;
    QPushButton *m_upButton;
    QPushButton *m_downButton;

    QMetaObject::Connection m_editConnection;

    void setupUI();
    void connectSignals();
    void editRowWithEmptyCheck(int row);

Q_SIGNALS:
    void pathsChanged();

private Q_SLOTS:
    void onAddClicked();
    void onDeleteClicked();
    void onUpClicked();
    void onDownClicked();
    void onListDoubleClicked(const QModelIndex &index);
    void onEmptyDoubleClicked(const QPoint &pos);
};
#endif // PATHEDITOR_H
