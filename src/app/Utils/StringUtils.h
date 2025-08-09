#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <filesystem>
#include <string>

#include <QString>

namespace StringUtils {
    using native_string =
#if defined(Q_OS_WIN)
        std::wstring;
#else
        std::string;
#endif

    inline native_string qstr_to_native(const QString &s) {
#if defined(Q_OS_WIN)
        return s.toStdWString();
#else
        return s.toStdString();
#endif
    }

    inline QString native_to_qstr(const native_string &s) {
#if defined(Q_OS_WIN)
        return QString::fromStdWString(s);
#else
        return QString::fromStdString(s);
#endif
    }

    inline std::filesystem::path qstr_to_path(const QString &s) {
        return {qstr_to_native(s)};
    }

    inline QString path_to_qstr(const std::filesystem::path &p) {
#if defined(Q_OS_WIN)
        return QString::fromStdWString(p.wstring());
#else
        return QString::fromStdString(p.string());
#endif
    }
}

#endif // STRINGUTILS_H