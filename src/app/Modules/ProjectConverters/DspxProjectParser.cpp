#include "DspxProjectParser.h"

#include <opendspx/model.h>
#include <opendspxserializer/serializer.h>

#include <exception>
#include <sstream>
#include <QCoreApplication>

DspxParseResult DspxProjectParser::parse(const QByteArray &data) {
    try {
        opendspx::SerializationErrorList errors;
        std::stringstream stream(data.toStdString(), std::ios::in);
        auto model = std::make_unique<opendspx::Model>(
            opendspx::Serializer::deserialize(stream, errors, opendspx::Serializer::CheckError));

        if (!errors.containsFatal())
            return {std::move(model), {}};

        QString errorDetails;
        for (const auto &error : errors)
            errorDetails += QCoreApplication::translate("DspxProjectParser", "Error type: %1\n")
                                .arg(error->type());
        return {nullptr, QCoreApplication::translate("DspxProjectParser",
                                                     "Failed to parse project file.\nerrors: %1")
                             .arg(errorDetails)};
    } catch (const std::exception &exception) {
        return {nullptr,
                QCoreApplication::translate("DspxProjectParser", "Failed to parse project file: %1")
                    .arg(exception.what())};
    } catch (...) {
        return {nullptr, QCoreApplication::translate(
                             "DspxProjectParser", "Failed to parse project file: unknown error")};
    }
}
