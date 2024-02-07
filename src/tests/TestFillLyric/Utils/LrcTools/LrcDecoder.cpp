#include "LrcDecoder.h"
#include "LrcDecoder_p.h"

#include <QFile>
#include <QDebug>
#include <QTextStream>

namespace LrcTools {

    static const QMap<QString, QString> MetaData = {
        {"ti", "title"          },
        {"al", "album"          },
        {"ar", "artist"         },
        {"au", "author"         },
        {"by", "creator"        },
        {"re", "encoder"        },
        {"ve", "encoder_version"}
    };

    static QString findMeta(const QString &tag) {
        QString data;
        if (MetaData.contains(tag))
            data = MetaData[tag];

        return data;
    }

    LrcDecoder::LrcDecoder(QObject *parent) : LrcDecoder(*new LrcDecoderPrivate(), parent) {
        d_ptr->q_ptr = this;
    }

    LrcDecoder::~LrcDecoder() = default;

    LrcDecoder::LrcDecoder(LrcDecoderPrivate &d, QObject *parent) : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
    }

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

        qint64 index = d->decodeHeader();

        if (index == d->m_lrcData.length()) {
            d->m_lastError = "No lyrics text!";
            return false;
        }

        d->m_lrcData.remove(0, index);

        QString line = d->readLine();
        while (!line.isEmpty()) {
            d->decodeLine(line);
            line = d->readLine();
        }

        d->m_duration = (--d->m_lyrics.end()).key();
        d->m_readIndex = d->m_lyrics.begin();

        return true;
    }

    QString LrcDecoder::get(const QString &meta) {
        Q_D(const LrcDecoder);
        QString data;
        if (d->m_metadata.contains(meta))
            data = d->m_metadata[meta];

        return data;
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

    bool LrcDecoder::seek(qint64 timestamp, LrcDecoder::SeekFlag flag) {
        Q_D(LrcDecoder);
        if (flag == SeekForward) {
            auto end = --d->m_lyrics.end();
            for (d->m_readIndex = d->m_lyrics.begin(); d->m_readIndex != end; ++d->m_readIndex) {
                if (d->m_readIndex.key() >= timestamp) {
                    return true;
                }
            }
        } else {
            for (d->m_readIndex = --d->m_lyrics.end(); d->m_readIndex != d->m_lyrics.begin();
                 --d->m_readIndex) {
                if (d->m_readIndex.key() <= timestamp) {
                    return true;
                }
            }
        }

        return false;
    }

    qint64 LrcDecoder::duration() const {
        Q_D(const LrcDecoder);
        return d->m_duration;
    }

    void LrcDecoder::dumpMetadata() {
        Q_D(const LrcDecoder);
        qDebug() << "[===Lyrics Metadata===]\n[Filename: " << d->m_filename << "]";
        if (d->m_metadata.isEmpty())
            qDebug() << "[No Metadata]";
        for (auto it = d->m_metadata.constBegin(); it != d->m_metadata.constEnd(); ++it) {
            qDebug() << "[" << it.key() << ": " << it.value() << "]";
        }
    }

    QStringList LrcDecoder::dumpLyrics() {
        Q_D(const LrcDecoder);
        QStringList lyrics;
        qDebug() << "[===Lyrics===]";
        for (auto it = d->m_lyrics.constBegin(); it != d->m_lyrics.constEnd(); ++it) {
            lyrics.append(it.value());
            qDebug() << "[Pts: " << it.key() << "]---[Lyric: " << it.value() << "]";
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
        qint64 length = m_lrcData.length();

        if (offset >= length)
            return offset;

        while (offset < length) {
            QString meta, data;
            if (m_lrcData.at(offset) == '[') {
                while (++offset < length && m_lrcData.at(offset) != ':') {
                    if (m_lrcData.at(offset).isLower())
                        meta += m_lrcData.at(offset);
                    else
                        return offset - 1;
                }

                while (++offset < length && m_lrcData.at(offset) != ']') {
                    data += m_lrcData.at(offset);
                }

                m_metadata[findMeta(meta)] = data;
            }

            offset++;
        }

        return offset;
    }

    void LrcDecoderPrivate::decodeLine(const QString &line) {
        qint64 offset = 0;
        qint64 length = line.length();

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
                pts += time.toLongLong() * 10; // 10 milliseconds
                times.append(pts);
                time.clear();
                pts = 0;
                offset++;
            } else if (line.at(offset) == ':') {
                // minute, = 60s * 1000ms
                pts += time.toLongLong() * 60 * 1000;
                time.clear();
                offset++;
            } else if (line.at(offset) == '.') {
                // second, = 1000 ms
                pts += time.toLongLong() * 1000;
                time.clear();
                offset++;
            } else {
                break;
            }
        }

        QString data;
        while (offset < length) {
            data += line.at(offset);
            offset++;
        }

        for (auto tempPts : times) {
            m_lyrics[tempPts] = data;
        }
    }

    QString LrcDecoderPrivate::readLine() {
        qint64 length = m_lrcData.length();
        QString line;
        while (m_currentIndex < length) {
            if (m_lrcData.at(m_currentIndex) == '\n') {
                m_currentIndex++;
                break;
            } else {
                line += m_lrcData.at(m_currentIndex);
                m_currentIndex++;
            }
        }

        return line;
    }
} // LrcTools