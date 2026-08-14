#ifndef DSSP_API_H
#define DSSP_API_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace DsspApi {

    struct Problem;

    /// Generic handler result: either a JSON response body or a problem.
    struct Result {
        int status = 200;
        QString contentType = QStringLiteral("application/json");
        QJsonValue body;
        bool isProblem = false;

        static Result ok(const QJsonObject &json);
        static Result ok(const QJsonArray &json);
        static Result fail(const Problem &problem);
    };

    // === Problem types (application/problem+json, aligned with the DSSP
    // reference implementation) ===
    struct ProblemType {
        QString uri;
        int httpStatus;
        QString title;
    };

    inline const ProblemType kUnknownArch{
        QStringLiteral("/problems/unknown_arch"), 404, QStringLiteral("Unknown arch")};
    inline const ProblemType kSingerNotExist{
        QStringLiteral("/problems/singer_not_exist"), 404, QStringLiteral("Singer not exist")};
    inline const ProblemType kSingerConfigInvalid{QStringLiteral("/problems/singer_config_invalid"),
                                                  422, QStringLiteral("Singer config invalid")};
    inline const ProblemType kInvalidParameter{QStringLiteral("/problems/invalid_parameter"), 422,
                                               QStringLiteral("Invalid parameter")};
    inline const ProblemType kSingersUnmixable{QStringLiteral("/problems/singers_unmixable"), 422,
                                               QStringLiteral("Singers unmixable")};
    inline const ProblemType kValidationError{
        QStringLiteral("/problems/validation_error"), 400, QStringLiteral("Validation error")};
    inline const ProblemType kNotImplemented{QStringLiteral("/problems/not_implemented"), 501,
                                             QStringLiteral("Not implemented")};
    inline const ProblemType kInternalError{QStringLiteral("/problems/internal_error"), 500,
                                            QStringLiteral("Internal error")};

    /// A machine-readable problem carrying the wire format fields.
    struct Problem {
        QString type;
        int status = 500;
        QString title;
        QString detail;
        QString arch;
        QString singer;
        QStringList singers;
        QString parameterId;
        QString parameterErrorType;
        QJsonArray validationErrors;
    };

    Problem problem(const ProblemType &type, const QString &detail = {});
    Problem unknownArch(const QString &arch);
    Problem singerNotExist(const QString &singer);
    Problem singerConfigInvalid(const QString &detail, const QJsonArray &errors = {});
    Problem invalidParameter(const QString &parameterId, const QString &errorType,
                             const QString &detail);
    Problem singersUnmixable(const QString &firstSinger, const QString &secondSinger,
                             const QString &detail = {});
    Problem validationError(const QString &detail);
    Problem notImplemented(const QString &detail = {});
    Problem internalError(const QString &detail);

    /// Serialize a problem to the wire format:
    /// {state:"ERROR", type, title, status, detail, arch?, singer?, singers?, parameter?, errors?}
    QJsonObject toJson(const Problem &problem);

    // === Small JSON helpers ===
    bool readObject(const QJsonObject &obj, const QString &key, QJsonObject &out);
    bool readArray(const QJsonObject &obj, const QString &key, QJsonArray &out);
    bool readString(const QJsonObject &obj, const QString &key, QString &out);
    bool readDouble(const QJsonObject &obj, const QString &key, double &out);
    bool readInt(const QJsonObject &obj, const QString &key, int &out);
    bool readBool(const QJsonObject &obj, const QString &key, bool &out);

    QString base64Encode(const QByteArray &data);
    /// Decode a "data:<mime>;base64,<payload>" URL. Returns false on failure.
    bool decodeDataUrl(const QString &url, QByteArray &data, QString &mimeType);

} // namespace DsspApi

#endif // DSSP_API_H
