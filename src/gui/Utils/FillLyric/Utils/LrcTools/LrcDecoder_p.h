#ifndef DS_EDITOR_LITE_LRCDECODER_P_H
#define DS_EDITOR_LITE_LRCDECODER_P_H

#include "LrcDecoder.h"

namespace LrcTools {
    class LrcDecoderPrivate {
        Q_DECLARE_PUBLIC(LrcDecoder)
    public:
        explicit LrcDecoderPrivate();
        ~LrcDecoderPrivate();

        qint64 m_duration = 0;
        qint64 m_currentIndex = 0;
        QString m_filename;
        QString m_lastError;
        QString m_lrcData;
        QMap<QString, QString> m_metadata;
        QMap<qint64, QString> m_lyrics;
        QMap<qint64, QString>::iterator m_readIndex;

        void cleanup();

        qint64 decodeHeader();
        void decodeLine(const QString &line);
        QString readLine();

    private:
        LrcDecoder *q_ptr{};
    };
} // LrcTools

#endif // DS_EDITOR_LITE_LRCDECODER_P_H
