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

    DrawCurve *curve(const int start, const QList<int> &values, const int step = 5) {
        auto *result = new DrawCurve;
        result->setLocalStart(start);
        result->step = step;
        result->setValues(values);
        return result;
    }

    void testBakeInterval() {
        QList<DrawCurve *> source{curve(0, {100, 110, 120, 130, 140})};
        QList<DrawCurve *> target{curve(0, {900, 900, 900, 900, 900})};

        expect(AppModelUtils::bakeDrawCurveRange(target, source, 20, 5),
               "source data in a reversed bake range must be copied");
        expect(target.size() == 1 && target.first()->localStart() == 0 &&
                   target.first()->values() == QList<int>({900, 110, 120, 130, 900}),
               "only the baked interval must overwrite edited values");

        QList<DrawCurve *> emptySource;
        expect(!AppModelUtils::bakeDrawCurveRange(target, emptySource, 0, 25),
               "a range without generated data must remain unchanged");
        expect(target.first()->values() == QList<int>({900, 110, 120, 130, 900}),
               "missing generated data must preserve edited values");

        qDeleteAll(source);
        qDeleteAll(target);
    }

    void testDifferentSampleSteps() {
        QList<DrawCurve *> source{curve(0, {100, 110, 120, 130, 140})};
        QList<DrawCurve *> target{curve(0, QList<int>(25, 900), 1)};

        expect(AppModelUtils::bakeDrawCurveRange(target, source, 5, 20),
               "curves with different sample steps must be merged");
        expect(target.size() == 1 && target.first()->localStart() == 0 &&
                   target.first()->step == 1 && target.first()->values().size() == 25,
               "the finer target sample grid must be preserved");
        if (target.size() == 1 && target.first()->values().size() == 25) {
            const auto &values = target.first()->values();
            expect(values.mid(0, 5) == QList<int>(5, 900) &&
                       values.mid(20, 5) == QList<int>(5, 900),
                   "values outside the baked interval must remain unchanged");
            expect(values.at(5) == 110 && values.at(10) == 120 && values.at(15) == 130,
                   "generated samples in the baked interval must be copied");
        }

        qDeleteAll(source);
        qDeleteAll(target);
    }

    void testUnalignedSourceSamples() {
        QList<DrawCurve *> source{curve(0, {0, 30, 60, 90, 120}, 3)};
        QList<DrawCurve *> target;

        expect(AppModelUtils::bakeDrawCurveRange(target, source, 5, 10),
               "a source grid not aligned to the bake range must be copied");
        expect(target.size() == 1 && target.first()->localStart() == 5 &&
                   target.first()->step == 1 &&
                   target.first()->values() == QList<int>({50, 60, 70, 80, 90}),
               "replacement samples must remain inside the baked interval");

        qDeleteAll(source);
        qDeleteAll(target);
    }

    void testSourceGapsStaySeparate() {
        QList<DrawCurve *> source{curve(0, {100, 110}), curve(20, {200, 210})};
        QList<DrawCurve *> target;

        expect(AppModelUtils::bakeDrawCurveRange(target, source, 0, 30),
               "all generated segments in the bake range must be copied");
        expect(target.size() == 2 && target.first()->localStart() == 0 &&
                   target.last()->localStart() == 20,
               "gaps in generated data must not be filled");

        qDeleteAll(source);
        qDeleteAll(target);
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testBakeInterval();
    testDifferentSampleSteps();
    testUnalignedSourceSamples();
    testSourceGapsStaySeparate();
    return failures == 0 ? 0 : 1;
}
