#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    int failures = 0;

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    DrawCurve *curve(const int start, const QList<int> &values) {
        auto *result = new DrawCurve;
        result->setLocalStart(start);
        result->setValues(values);
        return result;
    }

    void testOverwriteInterval() {
        QList<DrawCurve *> source{curve(0, {100, 110, 120, 130, 140})};
        QList<DrawCurve *> target{curve(0, {900, 900, 900, 900, 900})};

        expect(AppModelUtils::overwriteDrawCurveRange(target, source, 20, 5),
               "source data in a reversed brush range must be copied");
        expect(target.size() == 1 && target.first()->localStart() == 0 &&
                   target.first()->values() == QList<int>({900, 110, 120, 130, 900}),
               "only the brushed interval must overwrite edited values");

        QList<DrawCurve *> emptySource;
        expect(!AppModelUtils::overwriteDrawCurveRange(target, emptySource, 0, 25),
               "a range without generated data must remain unchanged");
        expect(target.first()->values() == QList<int>({900, 110, 120, 130, 900}),
               "missing generated data must preserve edited values");

        qDeleteAll(source);
        qDeleteAll(target);
    }

    void testSourceGapsStaySeparate() {
        QList<DrawCurve *> source{curve(0, {100, 110}), curve(20, {200, 210})};
        QList<DrawCurve *> target;

        expect(AppModelUtils::overwriteDrawCurveRange(target, source, 0, 30),
               "all generated segments in the brush range must be copied");
        expect(target.size() == 2 && target.first()->localStart() == 0 &&
                   target.last()->localStart() == 20,
               "gaps in generated data must not be filled");

        qDeleteAll(source);
        qDeleteAll(target);
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testOverwriteInterval();
    testSourceGapsStaySeparate();
    return failures == 0 ? 0 : 1;
}
