#ifndef HEADLESSTERMINATIONHANDLER_H
#define HEADLESSTERMINATIONHANDLER_H

#include <QString>

#include <functional>
#include <memory>

enum class HeadlessTerminationSignal {
    Interrupt,
    Terminate,
    ConsoleBreak,
};

[[nodiscard]] QString headlessTerminationSignalName(HeadlessTerminationSignal signal);

class HeadlessTerminationHandler final {
public:
    using Callback = std::function<bool(HeadlessTerminationSignal)>;

    HeadlessTerminationHandler();
    ~HeadlessTerminationHandler();

    HeadlessTerminationHandler(const HeadlessTerminationHandler &) = delete;
    HeadlessTerminationHandler &operator=(const HeadlessTerminationHandler &) = delete;

    bool start(Callback callback, QString &error);
    void stop();

private:
    struct Private;

    void handleSignal(HeadlessTerminationSignal signal);

    std::unique_ptr<Private> d;
};

#endif // HEADLESSTERMINATIONHANDLER_H
