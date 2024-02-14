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

        int end() const {
            return start + length;
        }
    };

public slots:
    void setTimeRange(double startTick, double endTick);
    void setTimeSignature(int numerator, int denominator) override;
    void setPosition(double tick);
    void setQuantize(int quantize) override;

private:
    void paintEvent(QPaintEvent *event) override;
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawEighth(QPainter *painter, int tick) override;
    double tickToX(double tick);
    double xToTick(double x);
    double m_startTick = 0;
    double m_endTick = 0;
    double m_position = 0;
    OverlapableSerialList<NoteViewModel> m_notes;
    QList<PhonemeViewModel *> m_phonemes;

    NoteViewModel *findNoteById(int id);
    void buildPhonemeList();
    void resetPhonemeList();
};



#endif // PHONEMEPARAMEDITOR_H
