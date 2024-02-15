//
// Created by fluty on 24-2-12.
//

#ifndef PHONEMEPARAMEDITOR_H
#define PHONEMEPARAMEDITOR_H

#include "Utils/FillLyric/PhonicWidget.h"
#include "Views/Utils/ITimelinePainter.h"
#include "Model/Note.h"
#include "Utils/OverlapableSerialList.h"

class PhonemeView final : public QWidget, public ITimelinePainter {
    Q_OBJECT

public:
    explicit PhonemeView(QWidget *parent = nullptr);
    void insertNote(Note *note);
    void removeNote(int noteId);
    void updateNote(Note *note);
    void reset();

    class NoteViewModel : public IOverlapable {
    public:
        int id = -1;
        int start = 0;
        int length = 0;
        QList<Phoneme> originalPhonemes;
        QList<Phoneme> editedPhonemes;

        int compareTo(NoteViewModel *obj) const {
            auto otherStart = obj->start;
            if (start < otherStart)
                return -1;
            if (start > otherStart)
                return 1;
            return 0;
        }
        bool isOverlappedWith(NoteViewModel *obj) const {
            auto otherStart = obj->start;
            auto otherEnd = otherStart + obj->length;
            auto curEnd = start + length;
            if (otherEnd <= start || curEnd <= otherStart)
                return false;
            return true;
        }
    };

    class PhonemeViewModel {
    public:
        enum PhonemeItemType { Ahead, Normal, Final, Sil };
        PhonemeItemType type;
        int noteId;
        int start;
        int length;
        QString name;

        PhonemeViewModel *prior;
        PhonemeViewModel *next;

        bool hoverOnControlBar = false;
        int startOffset = 0;
        int lengthOffset = 0;

        int end() const {
            return start + length;
        }
        int endWithOffset() const {
            return start + length + startOffset + lengthOffset;
        }
        bool canResizeRight() const {
            if (next == nullptr)
                return false;
            if (next->type == Sil)
                return false;
            if (start + length < next->start)
                return false;

            return true;
        }
        bool canResizeLeft() const {
            if (type == Sil && prior->type != Final)
                return false;
            return true;
        }
    };

signals:
    void adjustCompleted(PhonemeView::PhonemeViewModel *phonemeViewModel);

public slots:
    void setTimeRange(double startTick, double endTick);
    void setTimeSignature(int numerator, int denominator) override;
    void setPosition(double tick);
    void setQuantize(int quantize) override;

private:
    enum MouseMoveBehavior { ResizeLeft, ResizeRight, None };

    void paintEvent(QPaintEvent *event) override;
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawEighth(QPainter *painter, int tick) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void updateHoverEffects();
    bool eventFilter(QObject *object, QEvent *event) override;
    double tickToX(double tick);
    double xToTick(double x);
    double ticksPerPixel()const;
    bool canEdit() const;
    double m_startTick = 0;
    double m_endTick = 0;
    double m_resizeToleranceInTick = 0;
    double m_position = 0;
    OverlapableSerialList<NoteViewModel> m_notes;
    QList<PhonemeViewModel *> m_phonemes;
    MouseMoveBehavior m_mouseMoveBehavior = None;
    PhonemeViewModel *m_curPhoneme = nullptr;
    PhonemeViewModel *m_curOtherPhoneme = nullptr;
    int m_mouseDownX;
    int m_currentLengthInMs = 0;
    bool m_freezeHoverEffects = false;
    bool m_showDebugInfo = false;
    int m_canEditTicksPerPixelThreshold = 6;

    NoteViewModel *findNoteById(int id);
    PhonemeViewModel *phonemeAtTick(double tick);
    void buildPhonemeList();
    void resetPhonemeList();
    void clearHoverEffects(PhonemeViewModel *except = nullptr);
};



#endif // PHONEMEPARAMEDITOR_H
