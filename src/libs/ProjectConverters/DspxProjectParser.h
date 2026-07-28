#ifndef DSPXPROJECTPARSER_H
#define DSPXPROJECTPARSER_H

#include <QByteArray>
#include <QString>

#include <opendspx/model.h>

#include <memory>

struct DspxParseResult {
    std::unique_ptr<opendspx::Model> model;
    QString errorMessage;

    [[nodiscard]] bool success() const {
        return model != nullptr;
    }
};

class DspxProjectParser final {
public:
    static DspxParseResult parse(const QByteArray &data);
};

#endif // DSPXPROJECTPARSER_H
