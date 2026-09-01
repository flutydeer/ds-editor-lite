#ifndef LIBRESVIPCONVERTER_H
#define LIBRESVIPCONVERTER_H

#include <QByteArray>
#include <QString>

struct LibreSVIPConversionResult {
    QByteArray dspxData;
    QString errorMessage;

    [[nodiscard]] bool success() const {
        return !dspxData.isEmpty();
    }
};

class LibreSVIPConverter final {
public:
    static LibreSVIPConversionResult convertToDspx(const QString &executablePath,
                                                   const QString &inputPath);
};

#endif // LIBRESVIPCONVERTER_H
