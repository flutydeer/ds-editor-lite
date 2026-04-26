#include "LrcDecoder.h"
#include "LrcDecoder_p.h"

#include <QFile>
#include <QTextStream>

namespace LrcTools
{

    static const QMap<QString, QString> MetaData = {{"ti", "title"},          {"al", "album"},   {"ar", "artist"},
                                                    {"au", "author"},         {"by", "creator"}, {"re", "encoder"},
                                                    {"ve", "encoder_version"}};

    static QString findMeta(const QString &tag) { return MetaData.value(tag); }

    LrcDecoder::LrcDecoder(QObject *parent) : LrcDecoder(*new LrcDecoderPrivate(), parent) { d_ptr->q_ptr = this; }

    LrcDecoder::~LrcDecoder() = default;

    LrcDecoder::LrcDecoder(LrcDecoderPrivate &d, QObject *parent) : QObject(parent), d_ptr(&d) { d.q_ptr = this; }

    LrcDecoderPrivate::LrcDecoderPrivate() = default;

    LrcDecoderPrivate::~LrcDecoderPrivate() = default;

    bool LrcDecoder::decode(const QString &lrcFile) {
        Q_D(LrcDecoder);
        d->cleanup();

        d->m_filename = lrcFile;

        QFile file(lrcFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            d->m_lastError = "LRC decode error: Can't open file!";
            return false;
        }

        QTextStream in(&file);
        d->m_lrcData = in.readAll();
        file.close();

        if (d->m_lrcData.isEmpty()) {
            d->m_lastError = "LRC file is empty!";
            return false;
        }

        const qint64 index = d->decodeHeader();

        if (index == d->m_lrcData.length()) {
            d->m_lastError = "No lyrics text!";
            return false;
        }

        d->m_lrcData.erase(d->m_lrcData.cbegin(), d->m_lrcData.cbegin() + index);

        while (d->m_currentIndex < d->m_lrcData.length()) {
            const QString line = d->readLine();
            if (line.isEmpty())
                continue;
            d->decodeLine(line);
        }

        if (d->m_lyrics.isEmpty()) {
            d->m_lastError = "No valid lyrics lines found!";
            return false;
        }

        d->m_duration = (--d->m_lyrics.end()).key();
        d->m_readIndex = d->m_lyrics.begin();

        return true;
    }

    QString LrcDecoder::get(const QString &meta) {
        Q_D(const LrcDecoder);
        return d->m_metadata.value(meta);
    }

    LyricPacket LrcDecoder::readPacket() {
        Q_D(LrcDecoder);
        LyricPacket packet;
        if (d->m_readIndex != d->m_lyrics.end()) {
            packet.pts = d->m_readIndex.key();
            packet.lyric = d->m_readIndex.value();
            ++d->m_readIndex;
        }

        return packet;
    }

    bool LrcDecoder::seek(const qint64 &timestamp, const SeekFlag &flag) {
        Q_D(LrcDecoder);
        if (d->m_lyrics.isEmpty())
            return false;

        if (flag == SeekForward) {
            for (d->m_readIndex = d->m_lyrics.begin(); d->m_readIndex != d->m_lyrics.end(); ++d->m_readIndex) {
                if (d->m_readIndex.key() >= timestamp) {
                    return true;
                }
            }
        } else {
            d->m_readIndex = d->m_lyrics.end();
            --d->m_readIndex;
            while (true) {
                if (d->m_readIndex.key() <= timestamp) {
                    return true;
                }
                if (d->m_readIndex == d->m_lyrics.begin())
                    break;
                --d->m_readIndex;
            }
        }

        return false;
    }

    qint64 LrcDecoder::duration() const {
        Q_D(const LrcDecoder);
        return d->m_duration;
    }

    QStringList LrcDecoder::dumpMetadata() {
        Q_D(const LrcDecoder);
        QStringList metadata;
        metadata.append("[===Lyrics Metadata===]");
        metadata.append("[Filename: " + d->m_filename + "]");
        if (d->m_metadata.isEmpty())
            metadata.append("[No metadata found]");
        else {
            for (auto it = d->m_metadata.constBegin(); it != d->m_metadata.constEnd(); ++it) {
                metadata.append("[" + it.key() + ": " + it.value() + "]");
            }
        }
        metadata.append("[===End of Metadata===]");
        return metadata;
    }

    QStringList LrcDecoder::dumpLyrics() {
        Q_D(const LrcDecoder);
        QStringList lyrics;
        for (auto it = d->m_lyrics.constBegin(); it != d->m_lyrics.constEnd(); ++it) {
            lyrics.append(it.value());
        }
        return lyrics;
    }

    QString LrcDecoder::lastError() const {
        Q_D(const LrcDecoder);
        return d->m_lastError;
    }

    void LrcDecoderPrivate::cleanup() {
        m_currentIndex = 0;
        m_filename.clear();
        m_lastError.clear();
        m_lrcData.clear();
        m_metadata.clear();
        m_lyrics.clear();
    }

    qint64 LrcDecoderPrivate::decodeHeader() {
        qint64 offset = 0;
        const qint64 length = m_lrcData.length();

        if (offset >= length)
            return offset;

        while (offset < length) {
            if (m_lrcData.at(offset) == '[') {
                QString meta, data;
                while (++offset < length && m_lrcData.at(offset) != ':') {
                    if (m_lrcData.at(offset).isLower())
                        meta += m_lrcData.at(offset);
                    else
                        return offset - 1;
                }

                while (++offset < length && m_lrcData.at(offset) != ']') {
                    data += m_lrcData.at(offset);
                }

                const QString metaKey = findMeta(meta);
                if (!metaKey.isEmpty())
                    m_metadata[metaKey] = data;
            }

            offset++;
        }

        return offset;
    }

    void LrcDecoderPrivate::decodeLine(const QString &line) {
        qint64 offset = 0;
        const qint64 length = line.length();

        if (offset >= length)
            return;

        QString time;
        qint64 pts = 0;
        QList<qint64> times;
        while (offset < length) {
            if (line.at(offset) == '[') {
                offset++;
            } else if (line.at(offset).isDigit()) {
                time += line.at(offset);
                offset++;
            } else if (line.at(offset) == ']') {
                if (time.size() == 3) {
                    pts += time.toLongLong();
                } else {
                    pts += time.toLongLong() * 10;
                }
                times.append(pts);
                time.clear();
                pts = 0;
                offset++;
            } else if (line.at(offset) == ':') {
                pts += time.toLongLong() * 60 * 1000;
                time.clear();
                offset++;
            } else if (line.at(offset) == '.') {
                pts += time.toLongLong() * 1000;
                time.clear();
                offset++;
            } else {
                break;
            }
        }

        const QString data = line.mid(offset);

        for (auto tempPts : times) {
            m_lyrics[tempPts] = data;
        }
    }

    QString LrcDecoderPrivate::readLine() {
        const qint64 length = m_lrcData.length();
        if (m_currentIndex >= length)
            return {};

        const qint64 newlinePos = m_lrcData.indexOf('\n', m_currentIndex);
        if (newlinePos == -1) {
            auto line = m_lrcData.mid(m_currentIndex);
            m_currentIndex = length;
            if (line.endsWith('\r'))
                line.chop(1);
            return line;
        }

        auto line = m_lrcData.mid(m_currentIndex, newlinePos - m_currentIndex);
        m_currentIndex = newlinePos + 1;
        if (line.endsWith('\r'))
            line.chop(1);
        return line;
    }
} // namespace LrcTools
