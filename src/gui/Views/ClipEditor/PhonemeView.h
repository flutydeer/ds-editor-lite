//
// Created by fluty on 24-2-12.
//

#ifndef PHONEMEPARAMEDITOR_H
#define PHONEMEPARAMEDITOR_H

#include "Utils/FillLyric/PhonicWidget.h"
#include "Views/Utils/ITimelinePainter.h"
#include "Model/Note.h"

class PhonemeView final : public QWidget, public ITimelinePainter {
    Q_OBJECT

public:
    void insertNote(Note *note);
    void removeNote(int noteId);
    void updateNote(Note *note);
    void reset();

    class NoteViewModel {
    public:
        int id = -1;
        int start = 0;
        int length = 0;
        QList<Phoneme> originalPhonemes;
        QList<Phoneme> editedPhonemes;
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
    QList<NoteViewModel*> m_notes;

    NoteViewModel* findNoteById(int id);
};



#endif // PHONEMEPARAMEDITOR_H
