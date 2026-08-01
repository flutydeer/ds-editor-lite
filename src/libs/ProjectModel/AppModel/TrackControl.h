#ifndef DSTRACKCONTROL_H
#define DSTRACKCONTROL_H

#include <lite/Support/ISerializable.h>

class TrackControl : public ISerializable {
public:
    double gain() const;
    void setGain(double gain);
    double pan() const;
    void setPan(double pan);
    bool mute() const;
    void setMute(bool mute);
    bool solo() const;
    void setSolo(bool solo);

    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

private:
    double m_gain = 0.0;
    double m_pan = 0;
    bool m_mute = false;
    bool m_solo = false;
};

#endif // DSTRACKCONTROL_H