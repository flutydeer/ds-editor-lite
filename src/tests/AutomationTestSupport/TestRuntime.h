#ifndef AUTOMATIONTESTRUNTIME_H
#define AUTOMATIONTESTRUNTIME_H

#include "Automation/CoreRuntime.h"
#include "Interface/IEditorView.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <utility>

namespace AutomationTestSupport {

    inline Automation::EditorRuntimeServices editorServices(IEditorView *const *view) {
        Automation::EditorRuntimeServices services;
        services.captureView = [view] {
            return *view ? std::optional((*view)->captureEditorViewState()) : std::nullopt;
        };
        services.restoreView = [view](const EditorViewState &state) {
            return *view && (*view)->restoreEditorViewState(state);
        };
        services.centerTrackPanel = [view](const double tick, const double trackIndex) {
            return *view && (*view)->centerTrackPanelAt(tick, trackIndex);
        };
        services.setTrackPanelScale = [view](const double horizontal, const double vertical) {
            return *view && (*view)->setTrackPanelScale(horizontal, vertical);
        };
        services.setPanelVisibility = [view](const bool trackVisible, const bool bottomVisible) {
            return *view && (*view)->setEditorPanelVisibility(trackVisible, bottomVisible);
        };
        services.showBottomPanelPage = [view](const QString &pageId) {
            return *view && (*view)->showBottomPanelPage(pageId);
        };
        services.centerPianoRoll = [view](const double tick, const double keyIndex) {
            return *view && (*view)->centerPianoRollAt(tick, keyIndex);
        };
        services.setPianoRollScale = [view](const double horizontal, const double vertical) {
            return *view && (*view)->setPianoRollScale(horizontal, vertical);
        };
        services.setPianoRollEditMode = [view](const EditorViewGlobal::PianoRollEditMode mode) {
            return *view && (*view)->setPianoRollEditMode(mode);
        };
        services.revealFocus = [view](const HistoryFocus &focus, const bool finalize) {
            return *view &&
                   (finalize ? (*view)->finalizeFocus(focus) : (*view)->revealFocus(focus));
        };
        return services;
    }

    class TestRuntime final {
    public:
        explicit TestRuntime(Automation::EditorRuntimeServices editorServices = {},
                             Automation::DocumentRuntimeServices documentServices = {},
                             Automation::FileRuntimeServices fileServices = {},
                             Automation::AudioExportRuntimeServices audioExportServices = {},
                             Automation::PackageRuntimeServices packageServices = {},
                             Automation::PlaybackRuntimeServices playbackServices = {},
                             Automation::ApplicationRuntimeServices applicationServices = {})
            : m_history(resetHistory()),
              m_runtime(&m_model, m_history, std::move(documentServices),
                        std::move(playbackServices), std::move(editorServices), {}, {},
                        std::move(packageServices), {}, std::move(fileServices),
                        std::move(audioExportServices), {}, std::move(applicationServices)) {
        }

        ~TestRuntime() {
            m_history->reset();
        }

        Automation::CoreRuntime &runtime() {
            return m_runtime;
        }

        HistoryManager *history() const {
            return m_history;
        }

        AppModel &model() {
            return m_model;
        }

    private:
        static HistoryManager *resetHistory() {
            auto *history = HistoryManager::instance();
            history->reset();
            return history;
        }

        AppModel m_model;
        HistoryManager *m_history;
        Automation::CoreRuntime m_runtime;
    };

} // namespace AutomationTestSupport

#endif // AUTOMATIONTESTRUNTIME_H
