#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"

#include <lite/ProjectModel/AppModel/LoopSettings.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    class TestRun final {
    public:
        void expect(const bool condition, const char *scenario, const char *message) {
            ++m_assertions;
            if (condition)
                return;
            m_ok = false;
            QTextStream(stderr) << "FAILED [" << scenario << "]: " << message << Qt::endl;
        }

        [[nodiscard]] bool ok() const {
            return m_ok;
        }

        [[nodiscard]] int assertions() const {
            return m_assertions;
        }

    private:
        bool m_ok = true;
        int m_assertions = 0;
    };

    struct SignalCounters {
        int selectedTrack = 0;
        int activeClip = 0;
        int clipSelection = 0;
        int noteSelection = 0;
        int quantize = 0;
        int loop = 0;
    };

    // Views follow the AppStatus signals, so the reset has to publish every change.
    void countSignals(AppStatus *status, SignalCounters &counters) {
        QObject::connect(status, &AppStatus::selectedTrackIndexChanged, status,
                         [&counters](int) { ++counters.selectedTrack; });
        QObject::connect(status, &AppStatus::activeClipIdChanged, status,
                         [&counters](int) { ++counters.activeClip; });
        QObject::connect(status, &AppStatus::clipSelectionChanged, status,
                         [&counters](const QList<int> &) { ++counters.clipSelection; });
        QObject::connect(status, &AppStatus::noteSelectionChanged, status,
                         [&counters](const QList<int> &) { ++counters.noteSelection; });
        QObject::connect(status, &AppStatus::pianoRollQuantizeChanged, status,
                         [&counters](int) { ++counters.quantize; });
        QObject::connect(status, &AppStatus::loopSettingsChanged, status,
                         [&counters](const LoopSettings &) { ++counters.loop; });
    }

    void seedDocumentScopedState(AppStatus *status) {
        status->selectedTrackIndex = 2;
        status->activeClipId = 41;
        status->selectedClips = QList<int>{41, 42};
        status->primarySelectedClipId = 41;
        status->selectedNotes = QList<int>{7, 8};
        status->primarySelectedNoteId = 7;
        status->currentEditObject = AppStatus::EditObjectType::Param;
        status->pianoRollQuantize = 8;
        status->pianoRollQuantizeEnabled = false;
        status->trackAutoPageTurnEnabled = false;
        status->pianoRollAutoPageTurnEnabled = false;
        status->pianoRollVisibleRect = QRectF(1.0, 2.0, 3.0, 4.0);
        status->pianoRollNoteEditPreview = QVector<AppStatus::NoteEditPreview>{
            {9, 100, 200, 60}
        };
        status->pianoRollNoteErasePreview = QList<int>{9};
        status->projectEditableLength = 12345;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    TestRun test;
    constexpr auto scenario = "DOC-SCOPED-STATE-001";

    auto *status = AppStatus::instance();

    // State that belongs to the current document generation
    seedDocumentScopedState(status);
    // State that outlives a document switch
    const LoopSettings loop(true, 480, 960);
    status->loopSettings = loop;
    status->trackPanelCollapsed = true;
    status->bottomPanelCollapsed = true;
    status->trackAutoPageTurnAvailable = false;
    status->pianoRollAutoPageTurnAvailable = false;

    // Counters observe the reset only: the seeding above must not be counted
    SignalCounters counters;
    countSignals(status, counters);

    status->resetDocumentScopedState();

    test.expect(status->selectedTrackIndex == -1 && status->activeClipId == -1, scenario,
                "the reset must clear the selected track and the active clip");
    test.expect(status->selectedClips.get().isEmpty() && status->primarySelectedClipId == -1 &&
                    status->selectedNotes.get().isEmpty() && status->primarySelectedNoteId == -1,
                scenario, "the reset must clear both selections and their primary ids");
    test.expect(status->currentEditObject.get() == AppStatus::EditObjectType::None, scenario,
                "the reset must end any edit target left over from the previous document");
    test.expect(status->pianoRollNoteEditPreview.get().isEmpty() &&
                    status->pianoRollNoteErasePreview.get().isEmpty(),
                scenario, "the reset must drop the uncommitted piano roll previews");
    test.expect(status->pianoRollVisibleRect.get() == QRectF(), scenario,
                "the reset must invalidate the piano roll visible rect");
    test.expect(status->projectEditableLength == AppGlobal::ticksPerWholeNote * 100, scenario,
                "the reset must restore the default editable length");
    test.expect(status->pianoRollQuantize == 16 && status->pianoRollQuantizeEnabled.get(), scenario,
                "the reset must restore the default piano roll quantize state");
    test.expect(status->trackAutoPageTurnEnabled.get() &&
                    status->pianoRollAutoPageTurnEnabled.get(),
                scenario, "the reset must restore the default auto page turn state");

    test.expect(status->loopSettings.get() == loop, scenario,
                "the loop range comes from the new document and must not be cleared here");
    test.expect(status->trackPanelCollapsed.get() && status->bottomPanelCollapsed.get(), scenario,
                "the panel collapse state is a layout preference and must survive");
    test.expect(
        !status->trackAutoPageTurnAvailable.get() && !status->pianoRollAutoPageTurnAvailable.get(),
        scenario, "the auto page turn availability is derived from the views and must survive");

    test.expect(counters.selectedTrack == 1 && counters.activeClip == 1 &&
                    counters.clipSelection == 1 && counters.noteSelection == 1 &&
                    counters.quantize == 1 && counters.loop == 0,
                scenario,
                "the reset must publish exactly one change per cleared value and leave the loop "
                "signal alone");

    status->resetDocumentScopedState();
    test.expect(counters.selectedTrack == 1 && counters.activeClip == 1 &&
                    counters.clipSelection == 1 && counters.noteSelection == 1 &&
                    counters.quantize == 1,
                scenario, "the reset must be idempotent and silent when nothing has to change");

    QTextStream(stdout) << "TestDocumentScopedStateReset: " << test.assertions() << " assertions"
                        << Qt::endl;
    return test.ok() ? 0 : 1;
}
