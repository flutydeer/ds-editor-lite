//
// Created by fluty on 24-2-12.
//

#ifndef PHONEMEPARAMEDITOR_H
#define PHONEMEPARAMEDITOR_H

#include <QWidget>

#include "UI/Utils/ITimelinePainter.h"
#include "Utils/OverlappableSerialList.h"
#include "Utils/Overlappable.h"
#include "Model/AppModel/Clip.h"

class Note;
class Phoneme;

class PhonemeView final : public QWidget, public ITimelinePainter {
    Q_OBJECT

public:
    explicit PhonemeView(QWidget *parent = nullptr);
    void setDataContext(SingingClip *clip);

    class PhonemeViewModel {
    public:
        enum PhonemeItemType { Ahead, Normal, Final, Sil };
        PhonemeItemType type;
        int noteId;
        int start;
        QString name;

        PhonemeViewModel *prior;
        PhonemeViewModel *next;

        bool hoverOnControlBar = false;
        int startOffset = 0;
    };

signals:
    void adjustCompleted(PhonemeView::PhonemeViewModel *phonemeViewModel);

public slots:
    void setTimeRange(double startTick, double endTick);
    void setTimeSignature(int numerator, int denominator) override;
    void setPosition(double tick);
    void setQuantize(int quantize) override;

private slots:
    void onTempoChanged(double tempo);
    void onClipPropertyChanged();
    void onNoteChanged(SingingClip::NoteChangeType type, Note *note);
    void onNotePropertyChanged(Note::NotePropertyType type, Note *note);
    // void onNoteSelectionChanged();

private:
    enum MouseMoveBehavior { Move, None };

    void paintEvent(QPaintEvent *event) override;
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawEighth(QPainter *painter, int tick) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

    void moveToSingingClipState(SingingClip *clip);
    void moveToNullClipState();
    void updateHoverEffects();
    void handleNoteInserted(Note *note, bool updateView);
    void handleNoteRemoved(Note *note, bool updateView);
    void updateNoteTime(Note *note);
    void reset();
    double tickToX(double tick);
    double xToTick(double x);
    [[nodiscard]] double ticksPerPixel() const;
    [[nodiscard]] bool canEdit() const;
    double m_startTick = 0;
    double m_endTick = 0;
    double m_resizeToleranceInTick = 0;
    double m_position = 0;
    QList<Note *> m_notes;
    QList<PhonemeViewModel *> m_phonemes;
    MouseMoveBehavior m_mouseMoveBehavior = None;
    PhonemeViewModel *m_curPhoneme = nullptr;
    int m_mouseDownX = 0;
    int m_currentLengthInMs = 0;
    bool m_freezeHoverEffects = false;
    bool m_showDebugInfo = false;
    int m_canEditTicksPerPixelThreshold = 6;

    SingingClip *m_clip = nullptr;

    PhonemeViewModel *phonemeAtTick(double tick);
    QList<PhonemeViewModel *> findPhonemesByNoteId(int noteId);
    void buildPhonemeList();
    void resetPhonemeList();
    void clearHoverEffects(PhonemeViewModel *except = nullptr);
    void handleAdjustCompleted(PhonemeViewModel *phonemeViewModel);
};



#endif // PHONEMEPARAMEDITOR_H
