//
// Created by fluty on 24-2-28.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow();

public slots:
    void onAllDone();

private:
    void closeEvent(QCloseEvent *event) override;

    bool m_isCloseRequested = false;
    bool m_isAllDone = false;
};



#endif // MAINWINDOW_H
