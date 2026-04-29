#ifndef DS_EDITOR_LITE_MIDITEXTCODECCONVERTER_H
#define DS_EDITOR_LITE_MIDITEXTCODECCONVERTER_H

#include <QByteArray>
#include <QList>
#include <QString>

class MidiTextCodecConverter {
public:
    struct CodecInfo {
        QByteArray identifier;
        QString displayName;
    };

    static QList<CodecInfo> availableCodecs();
    static QByteArray detectEncoding(const QByteArray &data);
    static QString decode(const QByteArray &data, const QByteArray &codec);
    static QByteArray encode(const QString &text, const QByteArray &codec);
    static QByteArray defaultCodec();
};

#endif // DS_EDITOR_LITE_MIDITEXTCODECCONVERTER_H
