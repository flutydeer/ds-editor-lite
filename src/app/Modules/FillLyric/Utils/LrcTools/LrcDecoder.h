#ifndef LYRIC_TAB_UTILS_LRC_DECODER_H
#define LYRIC_TAB_UTILS_LRC_DECODER_H

#include <QObject>
#include <QSharedPointer>
#include <QString>

namespace LrcTools
{

    struct LyricPacket {
        QString lyric;
        qint64 pts = 0;

        bool isEmpty() const { return lyric.isEmpty() && pts == 0; }
    };

    class LrcDecoderPrivate;
    class LrcDecoder final : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(LrcDecoder)
    public:
        enum SeekFlag { SeekForward = 1, SeekBackward };

        explicit LrcDecoder(QObject *parent = nullptr);
        ~LrcDecoder() override;

        bool decode(const QString &lrcFile);
        QString get(const QString &meta);
        LyricPacket readPacket();
        bool seek(const qint64 &timestamp, const SeekFlag &flag = SeekForward);
        qint64 duration() const;
        QStringList dumpMetadata();
        QStringList dumpLyrics();
        QString lastError() const;

    protected:
        explicit LrcDecoder(LrcDecoderPrivate &d, QObject *parent = nullptr);

        QSharedPointer<LrcDecoderPrivate> d_ptr;
    };

} // namespace LrcTools

#endif // LYRIC_TAB_UTILS_LRC_DECODER_H
