//
// Created by fluty on 2024/1/31.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "Views/ClipEditor/ClipEditorView.h"
#include "Views/TracksEditor/TracksView.h"

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow();

private:
    TracksView *m_tracksView;
    ClipEditorView *m_clipEditView;
};



#endif // MAINWINDOW_H
