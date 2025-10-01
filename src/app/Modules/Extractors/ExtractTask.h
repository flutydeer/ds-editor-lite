#ifndef EXTRACTTASK_H
#define EXTRACTTASK_H

#include "Modules/Task/Task.h"

class ExtractTask : public Task {
    Q_OBJECT

public:
    enum class ErrorCode {
        UnknownError = -2,
        Terminated = -1,
        Success = 0,
        InferEngineNotLoaded = 1,
        ModelNotLoaded = 2,
        ModelRunFailed = 3,
    };

    struct Input {
        int singingClipId = -1;
        int audioClipId = -1;
        QString audioPath;
        double tempo = 0;
    };

    explicit ExtractTask(Input input) : m_input(std::move(input)) {
    }

    const Input &input() const {
        return m_input;
    }

    ErrorCode errorCode() const {
        return m_errorCode;
    }

    QString errorMessage() const {
        return m_errorMessage;
    }

    bool success() const {
        return m_errorCode == ErrorCode::Success;
    }

protected:
    Input m_input;
    ErrorCode m_errorCode = ErrorCode::UnknownError;
    QString m_errorMessage;
};

#endif // EXTRACTTASK_H