#include "Controller/DocumentWorkflow/DocumentWorkflowPathUtils.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowRevisionGuard.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/History/ActionSequence.h>
#include "AppContext.h"
#include <lite/History/HistoryManager.h>
#include "Utils/ConditionalTransition.h"

#include <QCoreApplication>
#include <QDir>
#include <QScopeGuard>
#include <QState>
#include <QStateMachine>
#include <QTranslator>
#include <QtTest>

#include <memory>

template <>
AppStatus *AppContext::instance<AppStatus>() {
    return nullptr;
}

template <>
HistoryManager *AppContext::instance<HistoryManager>() {
    return nullptr;
}

namespace {
    struct ActionCounts {
        int executed = 0;
        int undone = 0;
        int destroyed = 0;
    };

    class CountingAction final : public IAction {
    public:
        explicit CountingAction(std::shared_ptr<ActionCounts> counts)
            : m_counts(std::move(counts)) {
        }

        ~CountingAction() override {
            ++m_counts->destroyed;
        }

        void execute() override {
            ++m_counts->executed;
        }

        void undo() override {
            ++m_counts->undone;
        }

    private:
        std::shared_ptr<ActionCounts> m_counts;
    };

    class TestActionSequence final : public ActionSequence {
    public:
        using ActionSequence::addAction;
        using ActionSequence::setName;
        using ActionSequence::setTranslatableName;
    };

    class TestTranslator final : public QTranslator {
    public:
        QString translate(const char *context, const char *sourceText, const char *,
                          int) const override {
            if (qstrcmp(context, "TestActionSequence") == 0 &&
                qstrcmp(sourceText, "Translatable action") == 0)
                return QStringLiteral("Translated action");
            return {};
        }
    };

    class TransitionEmitter final : public QObject {
        Q_OBJECT
    signals:
        void proceed();
    };
}

class TestDocumentWorkflow final : public QObject {
    Q_OBJECT

private slots:

    void init() {
        historyManager->reset(HistoryManager::ResetState::Saved);
    }

    void cleanup() {
        historyManager->reset(HistoryManager::ResetState::Saved);
    }

    void savePointBaseline() {
        QVERIFY(historyManager->isOnSavePoint());
        historyManager->reset(HistoryManager::ResetState::Unsaved);
        QVERIFY(!historyManager->isOnSavePoint());
        QVERIFY(!historyManager->canUndo());
        historyManager->setSavePoint();
        QVERIFY(historyManager->isOnSavePoint());
    }

    void importSequenceUndoRedoAndOwnership() {
        const auto counts = std::make_shared<ActionCounts>();
        auto sequence = new TestActionSequence;
        sequence->setName(QStringLiteral("Import MIDI"));
        sequence->addAction(new CountingAction(counts));
        sequence->execute();
        historyManager->record(sequence);
        QCOMPARE(counts->executed, 1);
        QVERIFY(historyManager->canUndo());
        QCOMPARE(historyManager->undoActionName(), QStringLiteral("Import MIDI"));
        QVERIFY(!historyManager->isOnSavePoint());

        historyManager->undo();
        QCOMPARE(counts->undone, 1);
        QVERIFY(!historyManager->canUndo());
        QVERIFY(historyManager->canRedo());
        QVERIFY(historyManager->isOnSavePoint());
        historyManager->redo();
        QCOMPARE(counts->executed, 2);
        QVERIFY(historyManager->canUndo());
        QVERIFY(!historyManager->canRedo());
        historyManager->reset(HistoryManager::ResetState::Saved);
        QCOMPARE(counts->destroyed, 1);
        QVERIFY(historyManager->isOnSavePoint());
    }

    void historyNamesFollowTranslator() {
        const auto counts = std::make_shared<ActionCounts>();
        auto sequence = new TestActionSequence;
        sequence->setTranslatableName(
            "TestActionSequence", QT_TRANSLATE_NOOP("TestActionSequence", "Translatable action"));
        sequence->addAction(new CountingAction(counts));
        historyManager->record(sequence);
        QCOMPARE(historyManager->undoActionName(), QStringLiteral("Translatable action"));
        TestTranslator translator;
        QCoreApplication::installTranslator(&translator);
        const auto removeTranslator =
            qScopeGuard([&] { QCoreApplication::removeTranslator(&translator); });
        QCOMPARE(historyManager->undoActionName(), QStringLiteral("Translated action"));
        historyManager->undo();
        QCOMPARE(historyManager->redoActionName(), QStringLiteral("Translated action"));
        QCoreApplication::removeTranslator(&translator);
        QCOMPARE(historyManager->redoActionName(), QStringLiteral("Translatable action"));
    }

    void guardedTransition_data() {
        QTest::addColumn<bool>("condition");
        QTest::newRow("condition-true") << true;
        QTest::newRow("condition-false") << false;
    }

    void guardedTransition() {
        QFETCH(bool, condition);
        QStateMachine machine;
        auto initial = new QState(&machine);
        auto whenTrue = new QState(&machine);
        auto whenFalse = new QState(&machine);
        machine.setInitialState(initial);
        TransitionEmitter emitter;
        auto trueTransition = new ConditionalTransition(&emitter, SIGNAL(proceed()),
                                                        [condition] { return condition; });
        trueTransition->setTargetState(whenTrue);
        initial->addTransition(trueTransition);
        auto falseTransition = new ConditionalTransition(&emitter, SIGNAL(proceed()),
                                                         [condition] { return !condition; });
        falseTransition->setTargetState(whenFalse);
        initial->addTransition(falseTransition);
        machine.start();
        QTRY_VERIFY(initial->active());
        emit emitter.proceed();
        QTRY_VERIFY(condition ? whenTrue->active() : whenFalse->active());
        QVERIFY(condition ? !whenFalse->active() : !whenTrue->active());
    }

    void suggestedSavePath_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << QStringLiteral("test") << QStringLiteral("test.dspx");
        QTest::newRow("multiple-dots")
            << QStringLiteral("song.v1") << QStringLiteral("song.v1.dspx");
        QTest::newRow("spaces") << QStringLiteral("New Project")
                                << QStringLiteral("New Project.dspx");
        QTest::newRow("existing-suffix")
            << QStringLiteral("draft.dspx") << QStringLiteral("draft.dspx");
        QTest::newRow("uppercase-suffix")
            << QStringLiteral("draft.DSPX") << QStringLiteral("draft.DSPX");
        QTest::newRow("unicode") << QStringLiteral("未命名作品")
                                 << QStringLiteral("未命名作品.dspx");
    }

    void suggestedSavePath() {
        QFETCH(QString, name);
        QFETCH(QString, expected);
        const auto folder = QDir::tempPath();
        QCOMPARE(DocumentWorkflowPathUtils::suggestedSavePath({}, folder, name),
                 QDir(folder).filePath(expected));
    }

    void existingSavePathIsPreserved() {
        const auto existing =
            QDir(QDir::tempPath()).filePath(QStringLiteral("existing.project.dspx"));
        QCOMPARE(DocumentWorkflowPathUtils::suggestedSavePath(existing, QStringLiteral("ignored"),
                                                              QStringLiteral("ignored")),
                 existing);
    }

    void projectPathCaseSensitivity() {
        const auto upper = QDir(QDir::tempPath()).filePath(QStringLiteral("Song.dspx"));
        const auto lower = QDir(QDir::tempPath()).filePath(QStringLiteral("song.dspx"));
#ifdef Q_OS_WIN
        QVERIFY(DocumentWorkflowPathUtils::projectPathsEqual(upper, lower));
#else
        QVERIFY(!DocumentWorkflowPathUtils::projectPathsEqual(upper, lower));
#endif
        QVERIFY(DocumentWorkflowPathUtils::projectPathsEqual(upper, upper));
    }

    void dirtyRevisionMustBeApprovedAgain() {
        DocumentWorkflowRevisionGuard guard;
        const Automation::DocumentVersion original{Automation::DocumentId::create(), 194};
        const Automation::DocumentVersion advanced{original.documentId, 225};
        QVERIFY(!guard.ensureApproved(original, false));
        QVERIFY(guard.ensureApproved(original, true));
        QVERIFY(guard.ensureApproved(original, false));
        QVERIFY(!guard.ensureApproved(advanced, false));
        guard.approve(advanced);
        QVERIFY(guard.approves(advanced));
        QVERIFY(!guard.approves(original));
        QVERIFY(!guard.approves({Automation::DocumentId::create(), advanced.revision}));
    }
};

QTEST_GUILESS_MAIN(TestDocumentWorkflow)
#include "main.moc"
