#pragma once

#include "Automation/AutomationTypes.h"
#include <QHash>
#include <QPointer>

class AppContext;
class DocumentWindowManager;
class PianoKeyboardView;
class SingingClip;

class DocumentMidiInput final : public QObject {
public:
    explicit DocumentMidiInput(DocumentWindowManager &windows);
    void setRecording(AppContext &context, bool record);
    bool recording(const AppContext &context) const;
    void stopRecording();

private:
    void noteOn(int key);
    void noteOff(int key);
    DocumentWindowManager &m_windows;
    Automation::DocumentId m_recording;
    QPointer<SingingClip> m_clip;
    struct HeldNote {
        int start;
        qint64 completedLoopTicks;
    };
    QHash<int, HeldNote> m_starts;
    QHash<int, QPointer<PianoKeyboardView>> m_keyboards;
    QMetaObject::Connection m_playbackConnection;
};
