//
// Created by fluty on 2024/1/27.
//

#ifndef DSCLIP_H
#define DSCLIP_H

#include <QSharedPointer>
#include <QObject>

#include "DsNote.h"
#include "DsParams.h"
#include "Utils/IOverlapable.h"
#include "Utils/UniqueObject.h"

class DsClip : public QObject, public IOverlapable, public UniqueObject {
    Q_OBJECT

public:
    enum ClipType { Audio, Singing, Generic };

    virtual ~DsClip() = default;

    virtual ClipType type() const {
        return Generic;
    }
    QString name() const;
    void setName(const QString &text);
    int start() const;
    void setStart(int start);
    int length() const;
    void setLength(int length);
    int clipStart() const;
    void setClipStart(int clipStart);
    int clipLen() const;
    void setClipLen(int clipLen);
    double gain() const;
    void setGain(double gain);
    bool mute() const;
    void setMute(bool mute);

    int compareTo(IOverlapable *obj) override;
    bool isOverlappedWith(IOverlapable *obj) override;

    class ClipPropertyChangedArgs {
    public:
        int id = -1;

        QString name;
        int start = 0;
        int length = 0;
        int clipStart = 0;
        int clipLen = 0;
        double gain = 0;
        bool mute = false;

        int trackIndex = 0;
    };
    class AudioClipPropertyChangedArgs : public ClipPropertyChangedArgs {
    public:
        QString path;
    };
    // signals:
    //     void propertyChanged();

protected:
    QString m_name;
    int m_start = 0;
    int m_length = 0;
    int m_clipStart = 0;
    int m_clipLen = 0;
    double m_gain = 0;
    bool m_mute = false;
};

class DsAudioClip final : public DsClip {
public:
    ClipType type() const override {
        return Audio;
    }
    QString path() const;
    void setPath(const QString &path);

private:
    QString m_path;
};

class DsSingingClip final : public DsClip {
public:
    ClipType type() const override {
        return Singing;
    }

    // QList<DsNote> notes() const;
    // void setNotes(const QList<DsNote> &notes);
    // void addNote(const Note &note);
    // void removeNote(const Note &note);
    // void clearNotes();
    QList<DsNote> notes;
    DsParams params;

private:
    QList<DsNote> m_notes;
    // DsParams m_params;
};

// using DsClipPtr = QSharedPointer<DsClip>;
// using DsSingingClipPtr = QSharedPointer<DsSingingClip>;
// using DsAudioClipPtr = QSharedPointer<DsAudioClip>;

#endif // DSCLIP_H
