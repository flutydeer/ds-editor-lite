//
// Created by fluty on 2024/1/27.
//

#ifndef DSCLIP_H
#define DSCLIP_H

#include "Utils/Overlappable.h"
#include "Utils/ISelectable.h"
#include "Utils/OverlappableSerialList.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Params.h"
#include "Global/AppGlobal.h"
#include "Interface/IClip.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Utils/Property.h"


class DrawCurve;
class Params;
class Note;

class Clip : public QObject, public IClip, public Overlappable, public ISelectable {
    Q_OBJECT

public:
    ~Clip() override = default;

    [[nodiscard]] ClipType clipType() const override {
        return Generic;
    }

    [[nodiscard]] QString name() const override;
    void setName(const QString &text) override;
    [[nodiscard]] int start() const override;
    void setStart(int start) override;
    [[nodiscard]] int length() const override;
    void setLength(int length) override;
    [[nodiscard]] int clipStart() const override;
    void setClipStart(int clipStart) override;
    [[nodiscard]] int clipLen() const override;
    void setClipLen(int clipLen) override;
    [[nodiscard]] double gain() const override;
    void setGain(double gain) override;
    [[nodiscard]] bool mute() const override;
    void setMute(bool mute) override;
    void notifyPropertyChanged();

    [[nodiscard]] int endTick() const;

    int compareTo(const Clip *obj) const;
    bool isOverlappedWith(Clip *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;

    class ClipCommonProperties {
    public:
        ClipCommonProperties() = default;
        explicit ClipCommonProperties(const IClip &clip);
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

    static void applyPropertiesFromClip(ClipCommonProperties &args, const IClip &clip);
};

class AudioClip final : public Clip {
    Q_OBJECT

public:
    class AudioClipProperties final : public ClipCommonProperties {
    public:
        AudioClipProperties() = default;
        explicit AudioClipProperties(const AudioClip &clip);
        explicit AudioClipProperties(const IClip &clip);
        QString path;
    };

    ~AudioClip() override;

    [[nodiscard]] ClipType clipType() const override {
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

    explicit SingingClip();
    ~SingingClip() override;

    [[nodiscard]] ClipType clipType() const override {
        return Singing;
    }

    [[nodiscard]] const OverlappableSerialList<Note> &notes() const;
    void insertNote(Note *note);
    void removeNote(Note *note);
    [[nodiscard]] Note *findNoteById(int id) const;
    [[nodiscard]] QList<Note *> selectedNotes() const;

    void notifyNoteChanged(NoteChangeType type, Note *note);
    void notifyNoteSelectionChanged();
    void notifyParamChanged(ParamBundle::ParamName name, Param::ParamType type);
    Property<AppGlobal::LanguageType> defaultLanguage = AppGlobal::unknown;
    ParamBundle params;
    // QList<VocalPart> parts();

    static void copyCurves(const QList<Curve *> &source, QList<Curve *> &target);
    static void copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target);

signals:
    void noteChanged(SingingClip::NoteChangeType type, Note *note);
    void noteSelectionChanged();
    void paramChanged(ParamBundle::ParamName name, Param::ParamType type);
    void defaultLanguageChanged(AppGlobal::LanguageType language);

private:
    OverlappableSerialList<Note> m_notes;
};

// using DsClipPtr = QSharedPointer<DsClip>;
// using DsSingingClipPtr = QSharedPointer<DsSingingClip>;
// using DsAudioClipPtr = QSharedPointer<DsAudioClip>;

#endif // DSCLIP_H
