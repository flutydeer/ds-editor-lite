#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <QString>

namespace StringUtils {
#if defined(Q_OS_WIN)
    inline std::wstring qstr_to_std(const QString &s) {
        return s.toStdWString();
    }
#else
    inline std::string qstr_to_std(const QString &s) {
        return s.toStdString();
    }
#endif

    inline std::filesystem::path qstr_to_path(const QString &s) {
        return std::filesystem::path(qstr_to_std(s));
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