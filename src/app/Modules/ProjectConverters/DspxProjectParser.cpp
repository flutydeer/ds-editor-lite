#include "DspxProjectParser.h"

#include <opendspx/model.h>
#include <opendspxserializer/serializer.h>

#include <exception>
#include <sstream>

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
            errorDetails += QString("Error type: %1\n").arg(error->type());
        return {nullptr, QString("Failed to parse project file.\nerrors: %1").arg(errorDetails)};
    } catch (const std::exception &exception) {
        return {nullptr, QString("Failed to parse project file: %1").arg(exception.what())};
    } catch (...) {
        return {nullptr, QStringLiteral("Failed to parse project file: unknown error")};
    }
}
