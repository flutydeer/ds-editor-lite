#ifndef ACTIONBUTTONSVIEW_H
#define ACTIONBUTTONSVIEW_H

#include <QSize>
#include <QWidget>

class ToolButton;

class ActionButtonsView final : public QWidget {
    Q_OBJECT

public:
    explicit ActionButtonsView(QWidget *parent = nullptr);

signals:
    void saveTriggered();
    void undoTriggered();
    void redoTriggered();

public slots:
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                           const QString &redoActionName);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslateUi();

    int m_contentHeight = 30;
    QSize m_iconSize = QSize(16, 16);

    ToolButton *m_btnSave;
    ToolButton *m_btnUndo;
    ToolButton *m_btnRedo;
    QString m_undoActionName;
    QString m_redoActionName;
};

#endif // ACTIONBUTTONSVIEW_H
