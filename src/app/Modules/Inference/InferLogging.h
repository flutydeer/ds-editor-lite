#ifndef INFERLOGGING_H
#define INFERLOGGING_H

#include <QLoggingCategory>

/// Logging categories of the inference pipeline states and tasks.
///
/// Both categories report the progress of every piece through every stage. These messages are
/// needed only to investigate a pipeline, so their debug level is disabled by default and is
/// enabled with QT_LOGGING_RULES, for example "infer.state.debug=true;infer.task.debug=true".
Q_DECLARE_LOGGING_CATEGORY(logInferState)
Q_DECLARE_LOGGING_CATEGORY(logInferTask)

#endif // INFERLOGGING_H
