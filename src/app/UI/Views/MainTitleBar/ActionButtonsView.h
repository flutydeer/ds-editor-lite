//
// Created by fluty on 2024/2/5.
//

#ifndef ACTIONBUTTONSVIEW_H
#define ACTIONBUTTONSVIEW_H

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
    // void updateView();
    // void onCanSaveChanged(bool b);
    void onUndoRedoChanged(bool canUndo, const QString &undoActionName, bool canRedo,
                           const QString &redoActionName);

private:
    int m_contentHeight = 30;

    QPushButton *m_btnSave;
    QPushButton *m_btnUndo;
    QPushButton *m_btnRedo;

    const QIcon icoSaveWhite = QIcon(":svg/icons/save_16_filled_white.svg");
    const QIcon icoUndoWhite = QIcon(":svg/icons/arrow_undo_16_filled_white.svg");
    const QIcon icoRedoWhite = QIcon(":svg/icons/arrow_redo_16_filled_white.svg");
};



#endif // ACTIONBUTTONSVIEW_H
