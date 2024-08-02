//
// Created by fluty on 24-8-1.
//

#ifndef NOTEPROPERTYEDITDIALOG_H
#define NOTEPROPERTYEDITDIALOG_H

#include "Model/Note.h"
#include "UI/Dialogs/Base/OKCancelDialog.h"
#include "opendspx/qdspxnote.h"

class Note;
class NotePropertyDialog final : public OKCancelDialog {
    Q_OBJECT

public:
    class NoteDialogResult {
    public:
        QString language;
        QString lyric;
        Pronunciation pronunciation;
        Phonemes phonemes;
    };

    explicit NotePropertyDialog(Note *note, QWidget *parent = nullptr);

private:
    Note *m_note;

    static void regroupPhonemes(const QList<Phoneme> &phonemes, QList<Phoneme> &ahead,
                                QList<Phoneme> &normal, QList<Phoneme> &final);
    static QString phonemesToString(const QList<Phoneme> &phonemes);
    static QList<Phoneme> phonemesFromString(Phoneme::PhonemeType type, const QString &names);
};



#endif // NOTEPROPERTYEDITDIALOG_H
