#ifndef TESTSUPPORT_TESTASSERTIONS_H
#define TESTSUPPORT_TESTASSERTIONS_H

#include <QtTest>

#include <source_location>

namespace TestSupport {
    // Non-void scenario helpers report QtTest failures at the original call site.
    inline bool expect(const bool condition, const char *message,
                       const std::source_location location = std::source_location::current()) {
        return QTest::qVerify(condition, "scenario expectation", message, location.file_name(),
                              static_cast<int>(location.line()));
    }

    inline bool expect(const bool condition, const QString &message,
                       const std::source_location location = std::source_location::current()) {
        return expect(condition, qPrintable(message), location);
    }
}

#endif // TESTSUPPORT_TESTASSERTIONS_H
