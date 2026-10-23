#ifndef INFERLOGGING_H
#define INFERLOGGING_H

#include <QLoggingCategory>

/// Debug categories enabled through QT_LOGGING_RULES, e.g. infer.state.debug=true.
Q_DECLARE_LOGGING_CATEGORY(logInferState)
Q_DECLARE_LOGGING_CATEGORY(logInferTask)

#endif // INFERLOGGING_H
