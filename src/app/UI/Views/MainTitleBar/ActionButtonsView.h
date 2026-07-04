//
// Created by fluty on 2024/2/5.
//

#ifndef ACTIONBUTTONSVIEW_H
#define ACTIONBUTTONSVIEW_H

#include <QSize>
#include <QWidget>

class QPushButton;

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

private:
    int m_contentHeight = 30;
    QSize m_iconSize = QSize(16, 16);

    QPushButton *m_btnSave;
    QPushButton *m_btnUndo;
    QPushButton *m_btnRedo;
};

#endif // ACTIONBUTTONSVIEW_H
