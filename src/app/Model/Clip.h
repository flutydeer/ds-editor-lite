//
// Created by fluty on 2024/1/27.
//

#ifndef DSCLIP_H
#define DSCLIP_H

#include <QObject>

#include "Utils/IOverlapable.h"
#include "Utils/ISelectable.h"
#include "Utils/OverlappableSerialList.h"
#include "Utils/UniqueObject.h"
#include "ClipboardDataModel/NotesParamsInfo.h"
#include "Params.h"
#include "Model/AudioInfoModel.h"

class Params;
class Note;

class Clip : public QObject, public IOverlapable, public UniqueObject, public ISelectable {
    Q_OBJECT

public:
    enum ClipType { Audio, Singing, Generic };

    ~Clip() override = default;

    [[nodiscard]] virtual ClipType type() const {
        return Generic;
    }
    [[nodiscard]] QString name() const;
    void setName(const QString &text);
    [[nodiscard]] int start() const;
    void setStart(int start);
    [[nodiscard]] int length() const;
    void setLength(int length);
    [[nodiscard]] int clipStart() const;
    void setClipStart(int clipStart);
    [[nodiscard]] int clipLen() const;
    void setClipLen(int clipLen);
    [[nodiscard]] double gain() const;
    void setGain(double gain);
    [[nodiscard]] bool mute() const;
    void setMute(bool mute);
    void notifyPropertyChanged();

    [[nodiscard]] int endTick() const;

    int compareTo(Clip *obj) const;
    bool isOverlappedWith(Clip *obj) const;

    class ClipCommonProperties {
    public:
        static ClipCommonProperties fromClip(Clip *clip);
        virtual ~ClipCommonProperties() = default;
        int id = -1;

        QString name;
        int start = 0;
        int length = 0;
        int clipStart = 0;
        int clipLen = 0;
        double gain = 0;
        bool mute = false;
    };

signals:
    void propertyChanged();

protected:
    QString m_name;
    int m_start = 0;
    int m_length = 0;
    int m_clipStart = 0;
    int m_clipLen = 0;
    double m_gain = 0;
    bool m_mute = false;

    static void applyPropertiesFromClip(ClipCommonProperties &args ,Clip *clip);
};

class AudioClip final : public Clip {
    Q_OBJECT

public:
    class AudioClipProperties final : public ClipCommonProperties {
    public:
        static AudioClipProperties fromClip(AudioClip *clip);
        QString path;
    };

    [[nodiscard]] ClipType type() const override {
        return Audio;
    }
    [[nodiscard]] QString path() const {
        return m_path;
    }
    void setPath(const QString &path) {
        m_path = path;
        emit propertyChanged();
    }
    [[nodiscard]] const AudioInfoModel &audioInfo() const {
        return m_info;
    }
    void setAudioInfo(const AudioInfoModel &audioInfo) {
        m_info = audioInfo;
        emit propertyChanged();
    }

private:
    QString m_path;
    AudioInfoModel m_info;
};

class SingingClip final : public Clip {
    Q_OBJECT

public:
    enum NoteChangeType { Inserted, Removed };

    class VocalPart : public UniqueObject {
    public:
        NotesParamsInfo info;
        QString audioCachePath;
    };

    [[nodiscard]] ClipType type() const override {
        return Singing;
    }

    [[nodiscard]] const OverlappableSerialList<Note> &notes() const;
    void insertNote(Note *note);
    void removeNote(Note *note);
    Note *findNoteById(int id);
    [[nodiscard]] QList<Note *> selectedNotes() const;

    void notifyNoteChanged(NoteChangeType type, Note *note);
    void notifyNoteSelectionChanged();
    void notifyParamChanged(ParamBundle::ParamName paramName, Param::ParamType paramType);

    ParamBundle params;
    // QList<VocalPart> parts();

    static void copyCurves(const OverlappableSerialList<Curve> &source,
                           OverlappableSerialList<Curve> &target);

signals:
    void noteChanged(SingingClip::NoteChangeType type, Note *note);
    void noteSelectionChanged();
    void paramChanged(ParamBundle::ParamName paramName, Param::ParamType paramType);

private:
    OverlappableSerialList<Note> m_notes;
    QList<VocalPart> m_parts;
};

// using DsClipPtr = QSharedPointer<DsClip>;
// using DsSingingClipPtr = QSharedPointer<DsSingingClip>;
// using DsAudioClipPtr = QSharedPointer<DsAudioClip>;

#endif // DSCLIP_H
