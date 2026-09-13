// Checks that the 2.3 to 2.4 voicebank converter produces what the newer loader requires.
//
// It cannot load the result here -- that needs the main line, which this tree does not link yet --
// so it checks the shape the loader insists on, key by key, against the same fixture the editor's
// own integration tests use. When the dependency switch happens, this should be replaced by
// actually opening the package, which is a stronger test than any amount of shape checking.

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

    int failures = 0;

    void expect(bool condition, const QString &what) {
        if (!condition) {
            QTextStream(stderr) << "FAIL: " << what << Qt::endl;
            ++failures;
        }
    }

    QJsonObject readObject(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const QString fixture = QStringLiteral(TEST_FIXTURE_DIR);
    const QString script = QStringLiteral(TEST_SCRIPT);
    if (!QDir(fixture).exists()) {
        QTextStream(stderr) << "no fixture at " << fixture << ", skipping" << Qt::endl;
        return 77;
    }

    QTemporaryDir out;
    expect(out.isValid(), "a temporary directory is needed");
    if (!out.isValid()) {
        return 1;
    }
    const auto converted = QDir(out.path()).filePath("converted");

    QProcess python;
    python.start("python3", {script, fixture, "--output", converted, "--language",
                             "cmn=vendor/lang:linguist/cmn"});
    expect(python.waitForFinished(60000), "the converter must finish");
    expect(python.exitCode() == 0,
           QString("the converter must succeed, said: ")
               + QString::fromUtf8(python.readAllStandardError()).left(400));

    // desc.json: the four fields the loader refuses a package without, and the entry shape.
    const auto desc = readObject(QDir(converted).filePath("desc.json"));
    expect(desc.value("$version").toString() == "1.0", "desc needs a manifest format version");
    expect(desc.value("version").toString() == "1.0.0.0", "the version is padded to four parts");
    expect(desc.value("compatVersion").toString() == "1.0.0.0",
           "a converted package claims compatibility with nothing older");
    expect(desc.value("runtimeLevel").toInt() == 1, "desc needs a runtime level");
    expect(!desc.contains("contributes"), "the older contributes key must be gone");

    const auto contributions = desc.value("contributions").toObject();
    const auto inferences = contributions.value("inference").toArray();
    expect(inferences.size() == 5, "five inference contributions");
    for (const auto &entry : inferences) {
        const auto object = entry.toObject();
        expect(object.contains("id") && object.contains("path"),
               "every entry is an id and a path");
    }
    expect(contributions.value("singer").toArray().size() == 1, "one singer contribution");

    // An inference declaration: the triple the interpreter matches on, and schema become exports.
    const auto acoustic = readObject(QDir(converted).filePath("inferences/acoustic/config.json"));
    expect(acoustic.value("interface").toString() == "org.openvpi.dsinfer.inference.Acoustic",
           "class becomes the contract interface");
    expect(acoustic.value("variant").toString() == "onnx", "and the variant it is matched on");
    expect(!acoustic.contains("class"), "the older class key must be gone");
    expect(!acoustic.contains("id"), "an id belongs to the package now, not the declaration");
    expect(!acoustic.contains("$version"), "the format version belongs to desc.json only");
    expect(acoustic.value("exports").toObject().contains("speakers"),
           "schema becomes exports, carrying what it carried");
    expect(!acoustic.value("configuration").toObject().contains("frameWidth"),
           "a key nothing reads any more is dropped rather than passed through");
    expect(acoustic.value("configuration").toObject().value("model").toString() == "acoustic.onnx",
           "paths inside configuration are untouched, because nothing moved");

    // The singer: imports become roles and references, and languages only appear when bound.
    const auto singer = readObject(QDir(converted).filePath("characters/fixture/config.json"));
    expect(singer.value("interface").toString() == "org.openvpi.dsinfer.singer.DiffSinger",
           "the singer contract");
    expect(singer.value("variant").toString() == "openvpi", "and its variant");

    const auto imports = singer.value("imports").toArray();
    QStringList roles;
    for (const auto &entry : imports) {
        const auto role = entry.toObject().value("role").toString();
        roles << role;
        expect(!entry.toObject().contains("inferenceId"),
               "the older inferenceId key must be gone");
        // A module in this package is referenced with a leading colon; a linguist is in another
        // package and must not be, which is the whole reason a language has to be named from
        // outside rather than derived.
        const auto reference = entry.toObject().value("ref").toString();
        if (role.startsWith("singer/")) {
            expect(reference.startsWith(":"), "an inference here is referenced with a colon");
        } else {
            expect(!reference.startsWith(":") && reference.contains(':'),
                   "a linguist elsewhere is referenced by package and module");
        }
    }
    for (const auto &wanted : {"singer/duration", "singer/pitch", "singer/variance",
                               "singer/acoustic", "singer/vocoder"}) {
        expect(roles.contains(wanted), QString("the singer must import %1").arg(wanted));
    }

    const auto configuration = singer.value("configuration").toObject();
    expect(configuration.contains("dict"), "the singer provider requires a dict");
    // One language was bound on the command line and one was not. The bound one becomes an import
    // and a map entry; the other is dropped, because there is nothing on the newer line for the
    // older line's G2P settings to become.
    expect(roles.contains("lang/cmn"), "a bound language becomes an import");
    // The map and its default are category fields in the declaration root, where synthrt's
    // singer category reads them; a map left inside configuration would load as nothing.
    expect(singer.value("languages").toObject().value("cmn").toString() == "lang/cmn",
           "and a map entry naming that role in the declaration root");
    expect(singer.value("defaultLanguage").toString() == "cmn",
           "a singer declaring languages must name a default among them");
    expect(!configuration.contains("languages") && !configuration.contains("defaultLanguage"),
           "the map does not also ride in configuration");
    expect(!roles.contains("lang/eng"), "an unbound language is dropped, not invented");

    return failures == 0 ? 0 : 1;
}
