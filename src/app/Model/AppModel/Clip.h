//
// Created by fluty on 2024/1/27.
//

#ifndef CLIP_H
#define CLIP_H

#include "Interface/IClip.h"
#include "Utils/Overlappable.h"
#include "Utils/OverlappableSerialList.h"

#include <QObject>
#include <QMap>

class Clip : public QObject, public IClip, public Overlappable {
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

    QMap<QString, QJsonObject> &workspace();
    QMap<QString, QJsonObject> workspace() const;

    [[nodiscard]] int endTick() const;

    int compareTo(const Clip *obj) const;
    bool isOverlappedWith(const Clip *obj) const;
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
    QMap<QString, QJsonObject> m_workspace;

    static void applyPropertiesFromClip(ClipCommonProperties &args, const IClip &clip);
};

#endif // CLIP_H
