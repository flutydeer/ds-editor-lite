//
// Created by fluty on 24-7-4.
//

#include "../../app/Utils/Expected.h"

#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>

enum class ErrorType {
    UserCanceled,
    PermissionDenied,
    InvalidFormat,
    FileNotFound,
};

class TaskResult {
public:
    int count = 0;
    QList<double> data;
};

QString errorTypeToString(ErrorType errorType) {
    switch (errorType) {
        case ErrorType::UserCanceled:
            return "User canceled";
        case ErrorType::PermissionDenied:
            return "Permission denied";
        case ErrorType::InvalidFormat:
            return "Invalid format";
        case ErrorType::FileNotFound:
            return "File not found";
    }
    return "";
}

Expected<TaskResult, ErrorType> testExpected(const QString &path) {
    if (path == "")
        return ErrorType::UserCanceled;
    if (path == "test1")
        return ErrorType::PermissionDenied;
    if (path == "test2")
        return ErrorType::InvalidFormat;
    if (path == "valid")
        return TaskResult{
            3, {0.1, 0.2, 0.3}
        };
    return ErrorType::FileNotFound;
}

int main(int argc, char *argv[]) {
    qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    QApplication a(argc, argv);

    if (auto result1 = testExpected(""))
        qDebug() << "result1 success:" << result1.get().count;
    else
        qDebug() << "result1 failed" << errorTypeToString(result1.getError());

    auto result2 = testExpected("test1");
    if (result2.isPresent())
        qDebug() << "result2 success:" << result2.get().count;
    else
        qDebug() << "result2 failed" << errorTypeToString(result2.getError());
    try {
        qDebug() << "Try to get result2..." << result2.get().count;
    } catch (const std::exception &e) {
        qCritical() << e.what();
    }

    auto result3 = testExpected("test2").orElse({-1, {}}); // Failed, return default value
    qDebug() << "test2 result" << result3.count;

    auto result4 = testExpected("valid")
                       .onSuccess([&](const TaskResult &result) {
                           qDebug() << "valid success" << result.count;
                       })
                       .onFailure([&](const ErrorType &error) {
                           qDebug() << "valid failed" << errorTypeToString(error);
                       });
    try {
        qDebug() << "Try to get valid..." << errorTypeToString(result4.getError());
    } catch (const std::exception &e) {
        qCritical() << e.what();
    }

    return 0;
}