#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/DocumentWorkflow/IDocumentWorkflowUi.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest>

#include <functional>

namespace {
    class SavePrompt final : public IDocumentWorkflowUi {
    public:
        QWidget *documentWorkflowParentWidget() override {
            return nullptr;
        }

        SaveDecision askDocumentSaveDecision() override {
            ++decisionCalls;
            promptsWereBusy &= documentWorkflowController->busy();
            if (duringPrompt)
                duringPrompt();
            return decisions.isEmpty() ? SaveDecision::Cancel : decisions.takeFirst();
        }

        QString chooseDocumentSavePath(const QString &suggestion) override {
            suggestedPath = suggestion;
            ++pathCalls;
            return savePath;
        }

        bool confirmOpenWithoutPackageMetadata() override {
            return false;
        }

        void showDocumentWorkflowError(const ProjectOperationError &error) override {
            errors.append(error);
        }

        void showDocumentWorkflowBusy() override {
            ++busyCalls;
        }

        QList<SaveDecision> decisions;
        QString savePath;
        QString suggestedPath;
        QList<ProjectOperationError> errors;
        int decisionCalls = 0;
        int pathCalls = 0;
        int busyCalls = 0;
        bool promptsWereBusy = true;
        std::function<void()> duringPrompt;
    };
}

void ApplicationGuiTests::newDocumentHonorsTheSaveDecision_data() {
    QTest::addColumn<QString>("choice");
    QTest::newRow("cancel-preserves-dirty-project") << QStringLiteral("cancel");
    QTest::newRow("discard-replaces-project") << QStringLiteral("discard");
    QTest::newRow("cancel-save-path-preserves-project") << QStringLiteral("cancel-path");
    QTest::newRow("save-before-new") << QStringLiteral("save");
    QTest::newRow("failed-save-can-be-canceled") << QStringLiteral("failed-save");
}

void ApplicationGuiTests::newDocumentHonorsTheSaveDecision() {
    QFETCH(QString, choice);
    auto &runtime = *context->m_coreRuntime;
    Automation::TrackDraftDto track;
    track.name = QStringLiteral("Unsaved source");
    QVERIFY(runtime.project().insertTrack(commandContext(), 0, track));
    QVERIFY(!HistoryManager::instance()->isOnSavePoint());
    QVERIFY(!documentWorkflowController->busy());
    const auto before = runtime.documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    SavePrompt prompt;
    if (choice == QStringLiteral("cancel"))
        prompt.decisions = {SaveDecision::Cancel};
    else if (choice == QStringLiteral("discard"))
        prompt.decisions = {SaveDecision::Discard};
    else
        prompt.decisions = {SaveDecision::Save, SaveDecision::Cancel};
    if (choice == QStringLiteral("save") || choice == QStringLiteral("failed-save"))
        prompt.savePath = directory.filePath(QStringLiteral("source.dspx"));
    if (choice == QStringLiteral("failed-save"))
        QVERIFY(QDir().mkdir(prompt.savePath));
    if (choice == QStringLiteral("cancel"))
        prompt.duringPrompt = [] { documentWorkflowController->requestSaveAs(); };
    documentWorkflowController->setUi(&prompt);
    const auto clearUi = qScopeGuard([] { documentWorkflowController->setUi(nullptr); });
    QSignalSpy busy(documentWorkflowController, &DocumentWorkflowController::busyChanged);
    documentWorkflowController->requestNew();
    QTRY_VERIFY(!documentWorkflowController->busy());
    QVERIFY(prompt.promptsWereBusy);
    if (choice == QStringLiteral("cancel"))
        QCOMPARE(prompt.busyCalls, 1);
    QCOMPARE(busy.count(), 2);
    QCOMPARE(busy.first().first().toBool(), true);
    QCOMPARE(busy.last().first().toBool(), false);
    QCOMPARE(prompt.decisionCalls, choice == QStringLiteral("failed-save") ? 2 : 1);
    QCOMPARE(prompt.pathCalls, choice == QStringLiteral("save") ||
                                       choice == QStringLiteral("cancel-path") ||
                                       choice == QStringLiteral("failed-save")
                                   ? 1
                                   : 0);
    QCOMPARE(prompt.errors.size(), choice == QStringLiteral("failed-save") ? 1 : 0);
    const bool replaced = choice == QStringLiteral("discard") || choice == QStringLiteral("save");
    if (replaced) {
        QVERIFY(runtime.documentVersion().documentId != before.documentId);
        QVERIFY(HistoryManager::instance()->isOnSavePoint());
        QVERIFY(!HistoryManager::instance()->canUndo());
    } else {
        QCOMPARE(runtime.documentVersion(), before);
        QCOMPARE(context->m_appModel->serialize(), beforeModel);
        QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
        QVERIFY(!HistoryManager::instance()->isOnSavePoint());
    }
    if (choice == QStringLiteral("save")) {
        QVERIFY(QFileInfo(prompt.savePath).isFile());
        AppModel saved;
        DspxProjectConverter converter;
        QString error;
        QVERIFY2(converter.load(prompt.savePath, &saved, error, ImportMode::NewProject),
                 qPrintable(error));
        QVERIFY(!saved.tracks().isEmpty());
        QCOMPARE(saved.tracks().first()->name(), track.name);
        QVERIFY(documentWorkflowController->recentProjectFiles().contains(prompt.savePath));
    }
}
