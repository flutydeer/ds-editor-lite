#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStateMachine>
#include <QState>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLoadDataClicked();
    void onRetryClicked();
    void onExtractionTimerTimeout();
    void onParsingTimerTimeout();

private:
    void setupStateMachine();
    void setupUI();
    void updateStateDisplay();
    void startDataExtraction();
    void startDataParsing();

    QStateMachine *m_stateMachine;

    QState *m_processingState;
    QState *m_completedState;

    QState *m_extractingState;
    QState *m_extractionErrorState;
    QState *m_parsingState;

    QPushButton *m_loadDataButton;
    QPushButton *m_retryButton;
    QLabel *m_stateLabel;

    QTimer *m_extractionTimer;
    QTimer *m_parsingTimer;

    bool m_extractionSuccess;

signals:
    void extractionSuccess();
    void extractionFailed();
    void parsingCompleted();
    void dataChanged();
    void retryRequested();
};

#endif // MAINWINDOW_H
