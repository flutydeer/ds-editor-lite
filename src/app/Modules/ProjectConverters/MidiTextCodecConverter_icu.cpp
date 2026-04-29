#include "MidiTextCodecConverter.h"

#ifndef Q_OS_MAC

#ifdef Q_OS_WIN
#    include <icu.h>
#else
#    include <unicode/ucnv.h>
#    include <unicode/ucsdet.h>
#endif

#include <memory>

QList<MidiTextCodecConverter::CodecInfo> MidiTextCodecConverter::availableCodecs() {
    const auto count = ucnv_countAvailable();
    QList<CodecInfo> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        CodecInfo info;
        info.identifier = ucnv_getAvailableName(i);
        UErrorCode status = U_ZERO_ERROR;
        const auto *standardName = ucnv_getStandardName(info.identifier, "IANA", &status);
        if (!U_SUCCESS(status) || !standardName) {
            continue; // Skip non-IANA encodings to keep the list short.
        }
        info.displayName = QString::fromUtf8(standardName);
        result.append(info);
    }
    return result;
}

QByteArray MidiTextCodecConverter::detectEncoding(const QByteArray &data) {
    UErrorCode status = U_ZERO_ERROR;
    auto *detector = ucsdet_open(&status);
    if (U_FAILURE(status)) {
        return {};
    }

    std::unique_ptr<UCharsetDetector, void (*)(UCharsetDetector *)> defer(
        detector, [](UCharsetDetector *ptr) { ucsdet_close(ptr); });

    ucsdet_setText(detector, data.constData(), data.size(), &status);
    if (U_FAILURE(status)) {
        return {};
    }

    const UCharsetMatch *match = ucsdet_detect(detector, &status);
    if (U_FAILURE(status) || !match) {
        return {};
    }

    const char *charsetName = ucsdet_getName(match, &status);
    charsetName = ucnv_getCanonicalName(charsetName, "IANA", &status);
    if (!U_SUCCESS(status) || !charsetName) {
        return {};
    }

    return charsetName;
}

QString MidiTextCodecConverter::decode(const QByteArray &data, const QByteArray &codec) {
    if (data.isEmpty()) {
        return {};
    }

    const QByteArray codecName = codec.isEmpty() ? defaultCodec() : codec;

    UErrorCode status = U_ZERO_ERROR;
    UConverter *converter = ucnv_open(codecName.constData(), &status);
    if (!U_SUCCESS(status) || !converter) {
        return {};
    }
    ucnv_close(converter);

    status = U_ZERO_ERROR;
    const auto requiredLength = ucnv_convert("UTF16_PlatformEndian", codecName.constData(), nullptr, 0,
                                             data.constData(), data.size(), &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && !U_SUCCESS(status)) {
        return {};
    }

    QByteArray buffer;
    buffer.resize(requiredLength);

    status = U_ZERO_ERROR;
    ucnv_convert("UTF16_PlatformEndian", codecName.constData(), buffer.data(), requiredLength,
                 data.constData(), data.size(), &status);
    if (!U_SUCCESS(status)) {
        return {};
    }

    return QString::fromUtf16(reinterpret_cast<const char16_t *>(buffer.constData()),
                              buffer.size() / static_cast<int>(sizeof(char16_t)));
}

QByteArray MidiTextCodecConverter::encode(const QString &text, const QByteArray &codec) {
    if (text.isEmpty()) {
        return {};
    }

    const QByteArray codecName = codec.isEmpty() ? defaultCodec() : codec;

    UErrorCode status = U_ZERO_ERROR;
    UConverter *converter = ucnv_open(codecName.constData(), &status);
    if (!U_SUCCESS(status) || !converter) {
        return {};
    }
    ucnv_close(converter);

    const auto *source = reinterpret_cast<const char *>(text.utf16());
    const auto sourceLength = text.size() * static_cast<int>(sizeof(char16_t));

    status = U_ZERO_ERROR;
    const auto requiredLength = ucnv_convert(codecName.constData(), "UTF16_PlatformEndian", nullptr, 0,
                                             source, sourceLength, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && !U_SUCCESS(status)) {
        return {};
    }

    QByteArray buffer;
    buffer.resize(requiredLength);

    status = U_ZERO_ERROR;
    ucnv_convert(codecName.constData(), "UTF16_PlatformEndian", buffer.data(), requiredLength,
                 source, sourceLength, &status);
    if (!U_SUCCESS(status)) {
        return {};
    }

    return buffer;
}

QByteArray MidiTextCodecConverter::defaultCodec() {
    return "UTF-8";
}

#endif