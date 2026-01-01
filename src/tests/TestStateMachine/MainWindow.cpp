#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_stateMachine(nullptr), m_processingState(nullptr),
      m_completedState(nullptr), m_extractingState(nullptr), m_extractionErrorState(nullptr),
      m_parsingState(nullptr), m_loadDataButton(nullptr), m_retryButton(nullptr),
      m_stateLabel(nullptr), m_extractionTimer(new QTimer(this)), m_parsingTimer(new QTimer(this)),
      m_extractionSuccess(true) {
    setupUI();
    setupStateMachine();

    m_extractionTimer->setSingleShot(true);
    m_extractionTimer->setInterval(1500);
    connect(m_extractionTimer, &QTimer::timeout, this, &MainWindow::onExtractionTimerTimeout);

    m_parsingTimer->setSingleShot(true);
    m_parsingTimer->setInterval(1500);
    connect(m_parsingTimer, &QTimer::timeout, this, &MainWindow::onParsingTimerTimeout);
}

MainWindow::~MainWindow() {
    delete m_stateMachine;
}

void MainWindow::setupStateMachine() {
    m_stateMachine = new QStateMachine(this);

    m_processingState = new QState();
    m_completedState = new QState();

    m_extractingState = new QState(m_processingState);
    m_extractionErrorState = new QState(m_processingState);
    m_parsingState = new QState(m_processingState);

    m_processingState->setInitialState(m_extractingState);

    m_extractingState->assignProperty(m_stateLabel, "text", "State: Extracting Data");
    m_extractionErrorState->assignProperty(m_stateLabel, "text", "State: Extraction Error");
    m_parsingState->assignProperty(m_stateLabel, "text", "State: Parsing Data");
    m_completedState->assignProperty(m_stateLabel, "text", "State: Processing Completed");

    m_extractingState->assignProperty(m_retryButton, "enabled", false);
    m_extractionErrorState->assignProperty(m_retryButton, "enabled", true);
    m_parsingState->assignProperty(m_retryButton, "enabled", false);
    m_completedState->assignProperty(m_retryButton, "enabled", false);

    m_completedState->assignProperty(m_loadDataButton, "enabled", false);

    m_extractingState->addTransition(this, &MainWindow::extractionSuccess, m_parsingState);
    m_extractingState->addTransition(this, &MainWindow::extractionFailed, m_extractionErrorState);

    m_extractionErrorState->addTransition(this, &MainWindow::retryRequested, m_extractingState);

    m_parsingState->addTransition(this, &MainWindow::parsingCompleted, m_completedState);

    m_processingState->addTransition(this, &MainWindow::dataChanged, m_extractingState);

    connect(m_extractingState, &QState::entered, this, [this]() { startDataExtraction(); });

    connect(m_parsingState, &QState::entered, this, [this]() { startDataParsing(); });

    m_stateMachine->addState(m_processingState);
    m_stateMachine->addState(m_completedState);
    m_stateMachine->setInitialState(m_processingState);
}

void MainWindow::setupUI() {
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(50, 50, 50, 50);

    m_stateLabel = new QLabel("State: Ready", this);
    m_stateLabel->setAlignment(Qt::AlignCenter);
    m_stateLabel->setStyleSheet("QLabel { font-size: 18px; font-weight: bold; }");
    mainLayout->addWidget(m_stateLabel);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(20);

    m_loadDataButton = new QPushButton("Load Data", this);
    m_loadDataButton->setMinimumSize(150, 40);
    m_loadDataButton->setStyleSheet("QPushButton { font-size: 14px; }");
    connect(m_loadDataButton, &QPushButton::clicked, this, &MainWindow::onLoadDataClicked);
    buttonLayout->addWidget(m_loadDataButton);

    m_retryButton = new QPushButton("Retry", this);
    m_retryButton->setMinimumSize(150, 40);
    m_retryButton->setEnabled(false);
    m_retryButton->setStyleSheet("QPushButton { font-size: 14px; }");
    connect(m_retryButton, &QPushButton::clicked, this, &MainWindow::onRetryClicked);
    buttonLayout->addWidget(m_retryButton);

    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
}

void MainWindow::onLoadDataClicked() {
    if (!m_stateMachine->isRunning()) {
        m_stateMachine->start();
    } else {
        emit dataChanged();
    }
}

void MainWindow::onRetryClicked() {
    emit retryRequested();
}

void MainWindow::onExtractionTimerTimeout() {
    m_extractionSuccess = QRandomGenerator::global()->bounded(0, 10) < 7;
    if (m_extractionSuccess) {
        emit extractionSuccess();
    } else {
        emit extractionFailed();
    }
}

void MainWindow::onParsingTimerTimeout() {
    emit parsingCompleted();
}

void MainWindow::updateStateDisplay() {
    if (m_stateMachine->configuration().contains(m_extractingState)) {
        m_stateLabel->setText("State: Extracting Data");
    } else if (m_stateMachine->configuration().contains(m_extractionErrorState)) {
        m_stateLabel->setText("State: Extraction Error");
    } else if (m_stateMachine->configuration().contains(m_parsingState)) {
        m_stateLabel->setText("State: Parsing Data");
    } else if (m_stateMachine->configuration().contains(m_completedState)) {
        m_stateLabel->setText("State: Processing Completed");
    }
}

void MainWindow::startDataExtraction() {
    m_extractionTimer->start();
}

void MainWindow::startDataParsing() {
    m_parsingTimer->start();
}
