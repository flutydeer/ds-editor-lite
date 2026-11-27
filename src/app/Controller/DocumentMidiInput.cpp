#include "DocumentMidiInput.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Controller/PlaybackController.h"
#include "Controller/PianoRollNoteCommit.h"
#include "Model/AppModel/NewNoteDefaults.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/utils/MidiInputNoteMonitor.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h"
#include "UI/Window/DocumentWindowManager.h"
#include "UI/Window/MainWindow.h"
#include "UI/Window/DetachedPanelWindow.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/GUI/Utils/WindowDialogs.h>
#include <QApplication>
#include <limits>

DocumentMidiInput::DocumentMidiInput(DocumentWindowManager &windows) : m_windows(windows) {
    auto *midi = AudioSystem::midiSystem();
    auto *notes = midi->hardwareNotes();
    connect(notes, &MidiInputNoteMonitor::notePressed, this, &DocumentMidiInput::noteOn);
    connect(notes, &MidiInputNoteMonitor::noteReleased, this, &DocumentMidiInput::noteOff);
    const auto release = [this] {
        for (int key : m_keyboards.keys())
            noteOff(key);
    };
    connect(notes, &MidiInputNoteMonitor::allNotesReleased, this, release);
    connect(midi, &MidiSystem::deviceChanged, this, release);
}

bool DocumentMidiInput::recording(const AppContext &context) const {
    return m_clip && context.m_coreRuntime->documentVersion().documentId == m_recording;
}

void DocumentMidiInput::setRecording(AppContext &context, bool record) {
    if (m_clip && !recording(context))
        return;
    stopRecording();
    if (!record)
        return;
    const SingletonRegistry::Scope scope(context.services());
    auto *clip = qobject_cast<SingingClip *>(
        context.m_appModel->findClipById(context.m_appStatus->activeClipId));
    if (!clip)
        return;
    if (playbackController->playbackStatus() != PlaybackGlobal::Playing)
        playbackController->play();
    if (playbackController->playbackStatus() != PlaybackGlobal::Playing)
        return;
    m_clip = clip;
    m_recording = context.m_coreRuntime->documentVersion().documentId;
    m_playbackConnection = connect(playbackController, &PlaybackController::playbackStatusChanged,
                                   this, [this](PlaybackStatus status) {
                                       if (status != PlaybackGlobal::Playing)
                                           stopRecording();
                                   });
}

void DocumentMidiInput::stopRecording() {
    disconnect(m_playbackConnection);
    for (int key : m_starts.keys())
        noteOff(key);
    m_recording = {};
    m_clip.clear();
}

void DocumentMidiInput::noteOn(int key) {
    auto *entry = m_windows.find(m_recording);
    if (!entry || !m_clip) {
        entry = m_windows.active();
        const auto *focused = QApplication::activeWindow();
        if (!entry || !focused ||
            (focused != entry->window.get() && focused != entry->window->detachedPanelWindow()) ||
            WindowDialogs::blocks(focused))
            return;
    } else if (WindowDialogs::blocks(entry->window.get()) &&
               WindowDialogs::blocks(entry->window->detachedPanelWindow())) {
        return;
    }
    if (m_keyboards.contains(key))
        noteOff(key);
    auto *keyboard = entry->window->findChild<PianoKeyboardView *>();
    if (!keyboard && entry->window->detachedPanelWindow())
        keyboard = entry->window->detachedPanelWindow()->findChild<PianoKeyboardView *>();
    m_keyboards.insert(key, keyboard);
    if (keyboard)
        keyboard->setHeldKey(key, true);
    if (recording(*entry->context)) {
        const SingletonRegistry::Scope scope(entry->context->services());
        m_starts.insert(key, {qRound(playbackController->position()),
                              audioContext->completedLoopTicks()});
    }
}

void DocumentMidiInput::noteOff(int key) {
    if (auto keyboard = m_keyboards.take(key))
        keyboard->setHeldKey(key, false);
    if (!m_starts.contains(key))
        return;
    const auto held = m_starts.take(key);
    auto *entry = m_windows.find(m_recording);
    if (!entry || !m_clip)
        return;
    const SingletonRegistry::Scope scope(entry->context->services());
    const int localStart = held.start - m_clip->start();
    if (localStart < 0 || localStart == std::numeric_limits<int>::max())
        return;
    const auto duration = qRound64(playbackController->position()) - held.start +
                          (audioContext->completedLoopTicks() - held.completedLoopTicks);
    const qint64 maxLength = std::numeric_limits<int>::max() - localStart;
    const auto length = qBound(qint64{1}, duration, maxLength);
    const auto defaults = NewNoteDefaults::resolve(m_clip->singerInfo());
    (void) PianoRollNoteCommit::insert(
        *entry->context->m_coreRuntime, Automation::ClipId(m_clip->id()),
        {.localStart = localStart,
         .length = static_cast<int>(length),
         .keyIndex = key,
         .lyric = defaults.lyric,
         .language = defaults.language});
}
