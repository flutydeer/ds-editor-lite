//
// Created by fluty on 2024/1/31.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class TracksView;
class ClipEditorView;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow();

public slots:
    void onAllDone();

private:
    void closeEvent(QCloseEvent *event) override;

    bool m_isCloseRequested = false;
    bool m_isAllDone = false;

    TracksView *m_tracksView;
    ClipEditorView *m_clipEditView;
};



#endif // MAINWINDOW_H
