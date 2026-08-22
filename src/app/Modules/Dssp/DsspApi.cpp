#include "DsspApi.h"

namespace DsspApi {

    Result Result::ok(const QJsonObject &json) {
        Result r;
        r.body = json;
        return r;
    }

    Result Result::ok(const QJsonArray &json) {
        Result r;
        r.body = json;
        return r;
    }

    Result Result::fail(const Problem &problem) {
        Result r;
        r.status = problem.status;
        r.contentType = QStringLiteral("application/problem+json");
        r.body = toJson(problem);
        r.isProblem = true;
        return r;
    }

    Problem problem(const ProblemType &type, const QString &detail) {
        Problem p;
        p.type = type.uri;
        p.status = type.httpStatus;
        p.title = type.title;
        p.detail = detail;
        return p;
    }

    Problem unknownArch(const QString &arch) {
        auto p = problem(kUnknownArch, QStringLiteral("Unknown arch %1").arg(arch));
        p.arch = arch;
        return p;
    }

    Problem singerNotExist(const QString &singer) {
        auto p = problem(kSingerNotExist, QStringLiteral("Singer not exist: %1").arg(singer));
        p.singer = singer;
        return p;
    }

    Problem singerConfigInvalid(const QString &detail, const QJsonArray &errors) {
        auto p = problem(kSingerConfigInvalid, detail);
        p.validationErrors = errors;
        return p;
    }

    Problem invalidParameter(const QString &parameterId, const QString &errorType,
                             const QString &detail) {
        auto p = problem(kInvalidParameter, detail);
        p.parameterId = parameterId;
        p.parameterErrorType = errorType;
        return p;
    }

    Problem singersUnmixable(const QString &firstSinger, const QString &secondSinger,
                             const QString &detail) {
        auto p = problem(kSingersUnmixable, detail);
        p.singers = {firstSinger, secondSinger};
        return p;
    }

    Problem validationError(const QString &detail) {
        return problem(kValidationError, detail);
    }

    Problem notImplemented(const QString &detail) {
        return problem(kNotImplemented, detail);
    }

    Problem internalError(const QString &detail) {
        return problem(kInternalError, detail);
    }

    QJsonObject toJson(const Problem &problem) {
        QJsonObject obj{
            {QStringLiteral("state"), QStringLiteral("ERROR")},
            {QStringLiteral("type"), problem.type},
            {QStringLiteral("title"), problem.title},
            {QStringLiteral("status"), problem.status},
            {QStringLiteral("detail"), problem.detail},
        };
        if (!problem.arch.isEmpty())
            obj.insert(QStringLiteral("arch"), problem.arch);
        if (!problem.singer.isEmpty())
            obj.insert(QStringLiteral("singer"), problem.singer);
        if (!problem.singers.isEmpty())
            obj.insert(QStringLiteral("singers"), QJsonArray::fromStringList(problem.singers));
        if (!problem.parameterId.isEmpty()) {
            obj.insert(QStringLiteral("parameter"),
                       QJsonObject{
                           {QStringLiteral("id"), problem.parameterId},
                           {QStringLiteral("error_type"), problem.parameterErrorType},
                       });
        }
        if (!problem.validationErrors.isEmpty())
            obj.insert(QStringLiteral("errors"), problem.validationErrors);
        return obj;
    }

    bool readObject(const QJsonObject &obj, const QString &key, QJsonObject &out) {
        if (!obj.contains(key) || !obj.value(key).isObject())
            return false;
        out = obj.value(key).toObject();
        return true;
    }

    bool readArray(const QJsonObject &obj, const QString &key, QJsonArray &out) {
        if (!obj.contains(key) || !obj.value(key).isArray())
            return false;
        out = obj.value(key).toArray();
        return true;
    }

    bool readString(const QJsonObject &obj, const QString &key, QString &out) {
        if (!obj.contains(key) || !obj.value(key).isString())
            return false;
        out = obj.value(key).toString();
        return true;
    }

    bool readDouble(const QJsonObject &obj, const QString &key, double &out) {
        if (!obj.contains(key) || !obj.value(key).isDouble())
            return false;
        out = obj.value(key).toDouble();
        return true;
    }

    bool readInt(const QJsonObject &obj, const QString &key, int &out) {
        if (!obj.contains(key))
            return false;
        const auto value = obj.value(key).toDouble();
        if (static_cast<double>(static_cast<int>(value)) != value)
            return false;
        out = static_cast<int>(value);
        return true;
    }

    bool readBool(const QJsonObject &obj, const QString &key, bool &out) {
        if (!obj.contains(key) || !obj.value(key).isBool())
            return false;
        out = obj.value(key).toBool();
        return true;
    }

    QString base64Encode(const QByteArray &data) {
        return QString::fromLatin1(data.toBase64(QByteArray::Base64Encoding));
    }

    bool decodeDataUrl(const QString &url, QByteArray &data, QString &mimeType) {
        const auto prefix = QStringLiteral("data:");
        if (!url.startsWith(prefix))
            return false;
        const auto commaIndex = url.indexOf(',');
        if (commaIndex < 0)
            return false;
        const auto meta = url.mid(prefix.size(), commaIndex - prefix.size());
        mimeType = meta.section(';', 0, 0);
        const auto payload = url.mid(commaIndex + 1);
        if (meta.contains(QStringLiteral(";base64"))) {
            const auto bytes = QByteArray::fromBase64(payload.toLatin1());
            if (bytes.isEmpty() && !payload.isEmpty())
                return false;
            data = bytes;
            return true;
        }
        data = QByteArray::fromPercentEncoding(payload.toUtf8());
        return true;
    }

} // namespace DsspApi
