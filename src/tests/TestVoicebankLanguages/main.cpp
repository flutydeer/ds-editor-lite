// Tests the language conversion of the voicebank converter on fixtures written by this test.
//
// The rest of the conversion is a mechanical key mapping. The language conversion is not, because
// the source of the three stages of a language differs per language, and the converter must derive
// it from the installed packages. Three rules determine the binding, and each rule addresses a
// previously observed conversion error:
//
//   a shared engine is not the entry point of a language, although it declares languages
//   a voicebank that ships its own phoneme stage keeps it instead of binding past it
//   a voicebank that ships no stage binds to the complete linguist of the package, if one exists
//
// The fixtures are written by this test instead of stored in the tree, so that the fixture for each
// rule is in the same file as the assertion on it.

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

    void write(const QString &path, const QJsonObject &value) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return;
        }
        file.write(QJsonDocument(value).toJson());
    }

    void writeText(const QString &path, const QString &text) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return;
        }
        file.write(text.toUtf8());
    }

    QJsonObject read(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    QJsonObject entry(const QString &id, const QString &path) {
        return QJsonObject{{"id", id}, {"path", path}};
    }

    QStringList references(const QJsonObject &declaration, const QString &prefix) {
        QStringList result;
        for (const auto &item : declaration.value("imports").toArray()) {
            const auto role = item.toObject().value("role").toString();
            if (role.startsWith(prefix)) {
                result << role + "=" + item.toObject().value("ref").toString();
            }
        }
        result.sort();
        return result;
    }

    /// Writes a shared engine and two language packages, in the three package layouts that ship.
    void writeLanguagePackages(const QString &root) {
        // Fixture for the first rule in the file header (a shared engine is not an entry point).
        write(root + "/engine/desc.json", QJsonObject{
            {"$version", "1.0"}, {"id", "vendor/engine"}, {"version", "2.0.0.5"},
            {"compatVersion", "2.0.0.0"}, {"runtimeLevel", 1},
            {"contributions", QJsonObject{{"inference", QJsonArray{
                entry("multi", "./inferences/multi/inference.json")}}}},
        });
        write(root + "/engine/inferences/multi/inference.json", QJsonObject{
            {"interface", "org.openvpi.wolf.inference.G2P"}, {"level", 1},
            {"variant", "multig2p-onnx"},
            {"exports", QJsonObject{{"languages", QJsonArray{
                QJsonObject{{"language", "cmn"}, {"scheme", "pinyin"}},
                QJsonObject{{"language", "eng"}, {"scheme", "arpabet"}},
                QJsonObject{{"language", "zzz"}, {"scheme", "ds"}}}}}},
        });

        // Mandarin: a chain over the engine and nothing else. The phoneme stage of this language
        // is voicebank content, so the package intentionally has no linguist.
        write(root + "/lang-cmn/desc.json", QJsonObject{
            {"$version", "1.0"}, {"id", "vendor/lang-cmn"}, {"version", "1.0.1.3"},
            {"compatVersion", "1.0.1.0"}, {"runtimeLevel", 1},
            {"contributions", QJsonObject{{"inference", QJsonArray{
                entry("g2p", "./inferences/g2p/inference.json")}}}},
            {"dependencies", QJsonArray{QJsonObject{{"id", "vendor/engine"},
                                                    {"version", "2.0.0.0"}}}},
        });
        write(root + "/lang-cmn/inferences/g2p/inference.json", QJsonObject{
            {"interface", "org.openvpi.wolf.inference.G2P"}, {"level", 1},
            {"variant", "pipe-chain"},
            {"imports", QJsonArray{QJsonObject{{"role", "backend"},
                                               {"ref", "vendor/engine:inference/multi"}}}},
        });

        // English: a chain over the same engine and a complete linguist over that chain. The
        // English phonemes are the same for every voicebank, so the package can contain the
        // complete linguist.
        write(root + "/lang-eng/desc.json", QJsonObject{
            {"$version", "1.0"}, {"id", "vendor/lang-eng"}, {"version", "1.0.0.3"},
            {"compatVersion", "1.0.0.0"}, {"runtimeLevel", 1},
            {"contributions", QJsonObject{
                {"inference", QJsonArray{entry("g2p", "./inferences/g2p/inference.json"),
                                         entry("s2p", "./inferences/s2p/inference.json")}},
                {"linguist", QJsonArray{
                    entry("eng-arpabet", "./linguists/eng-arpabet/linguist.json")}}}},
            {"dependencies", QJsonArray{QJsonObject{{"id", "vendor/engine"},
                                                    {"version", "2.0.0.0"}}}},
        });
        write(root + "/lang-eng/inferences/g2p/inference.json", QJsonObject{
            {"interface", "org.openvpi.wolf.inference.G2P"}, {"level", 1},
            {"variant", "pipe-chain"},
            {"exports", QJsonObject{{"languages", QJsonArray{
                QJsonObject{{"language", "eng"}, {"scheme", "arpabet"}}}}}},
            {"imports", QJsonArray{QJsonObject{{"role", "backend"},
                                               {"ref", "vendor/engine:inference/multi"}}}},
        });
        write(root + "/lang-eng/inferences/s2p/inference.json", QJsonObject{
            {"interface", "org.openvpi.wolf.inference.S2P"}, {"level", 1}, {"variant", "direct"},
        });
        write(root + "/lang-eng/linguists/eng-arpabet/linguist.json", QJsonObject{
            {"interface", "org.openvpi.wolf.linguist.WolfLinguist"}, {"level", 1},
            {"variant", "wolf"}, {"language", "eng"}, {"scheme", "arpabet"},
            {"exports", QJsonObject{{"phonemes", QJsonArray{"aa", "b"}}}},
            {"imports", QJsonArray{QJsonObject{{"role", "linguist/g2p"},
                                               {"ref", ":inference/g2p"}}}},
        });
    }

    /// Writes a voicebank in the 2.3 format that declares three languages: one with its own phoneme
    /// stage, one without any stage, and one that no installed package supports.
    void writeVoicebank(const QString &root) {
        // AP is a marker known to every host; hum is a marker specific to this voicebank. Both
        // appear among the syllables and map to themselves, which is the form of a marker in such
        // a dictionary.
        writeText(root + "/assets/cmn.txt", "AP\tAP\nhum\thum\nma\tm a\nba\tb a\na\ta\n");
        writeText(root + "/assets/cmn_onset.json",
                  "{\"phonemeTypes\":{\"m\":\"consonant\",\"b\":\"consonant\",\"a\":\"vowel\","
                  "\"AP\":\"vowel\"},\"rules\":[{\"pattern\":[\"vowel\"],\"onsets\":[0]}]}");
        write(root + "/inferences/vocoder/config.json", QJsonObject{
            {"$version", "1.0"}, {"id", "vocoder"}, {"class", "ai.svs.VocoderInference"},
            {"level", 1}, {"configuration", QJsonObject{{"model", "vocoder.onnx"}}},
        });
        // The phoneme table records which phonemes of a voicebank belong to a language: `cmn/a`
        // belongs to cmn, and the bare `AP` belongs to no language. This makes AP a reserved
        // phoneme and `a` a language phoneme; the converter reads this from the table instead of
        // assuming it. `hum` is in no table, so it is neither reserved nor singable.
        write(root + "/inferences/acoustic/phonemes.json", QJsonObject{
            {"AP", 1}, {"cmn/a", 2}, {"cmn/b", 3}, {"cmn/m", 4},
        });
        write(root + "/inferences/acoustic/config.json", QJsonObject{
            {"$version", "1.0"}, {"id", "acoustic"}, {"class", "ai.svs.AcousticInference"},
            {"level", 1}, {"configuration", QJsonObject{{"model", "acoustic.onnx"},
                                                        {"phonemes", "phonemes.json"}}},
        });
        write(root + "/characters/singer/config.json", QJsonObject{
            {"$version", "1.0"}, {"id", "singer"}, {"class", "diffsinger"}, {"level", 1},
            {"imports", QJsonArray{QJsonObject{{"inferenceId", "acoustic"}},
                                   QJsonObject{{"inferenceId", "vocoder"}}}},
            {"configuration", QJsonObject{
                {"dict", "../../assets/cmn.txt"},
                {"defaultLanguage", "cmn"},
                {"languages", QJsonArray{
                    QJsonObject{{"id", "cmn"}, {"g2p", "g2p-cmn-official"},
                                {"s2pMode", "dict"}, {"s2pFile", "../../assets/cmn.txt"},
                                {"onsetMode", "rule"},
                                {"onsetFile", "../../assets/cmn_onset.json"}},
                    QJsonObject{{"id", "eng"}, {"g2p", "g2p-eng-official"}},
                    QJsonObject{{"id", "xyz"}, {"g2p", "g2p-xyz-official"}}}}}},
        });
        write(root + "/desc.json", QJsonObject{
            {"id", "fixture"}, {"version", "1.0"},
            {"contributes", QJsonObject{
                {"inferences", QJsonArray{"inferences/acoustic/config.json",
                                          "inferences/vocoder/config.json"}},
                {"singers", QJsonArray{"characters/singer/config.json"}}}},
        });
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QTemporaryDir work;
    expect(work.isValid(), "the temporary directory must be valid");
    if (!work.isValid()) {
        return 1;
    }
    const auto packages = QDir(work.path()).filePath("packages");
    const auto source = QDir(work.path()).filePath("voicebank");
    const auto converted = QDir(work.path()).filePath("converted");
    writeLanguagePackages(packages);
    writeVoicebank(source);

    QProcess python;
    python.start(QStringLiteral(TEST_PYTHON), {QStringLiteral(TEST_SCRIPT), source, "--output",
                                              converted, "--packages", packages});
    expect(python.waitForFinished(60000), "the converter must finish");
    const auto diagnostics = QString::fromUtf8(python.readAllStandardError());
    expect(python.exitCode() == 0, "the converter must succeed; stderr: " + diagnostics.left(600));

    const auto singer = read(QDir(converted).filePath("characters/singer/config.json"));
    // The map is a singer category field in the declaration root, where synthrt reads it.
    const auto languages = singer.value("languages").toObject();

    // Mandarin ships a dictionary and onset rules, so it keeps them: the linguist is built in the
    // converted package, and only its grapheme stage references the language package.
    expect(languages.contains("cmn"), "the language with its own stages must be bound");
    const auto cmnImports = references(singer, "lang/cmn");
    expect(cmnImports == QStringList{"lang/cmn=:linguist/cmn-pinyin"},
           "the language must be bound to a linguist in the converted package, got "
               + cmnImports.join(", "));
    const auto cmn = read(QDir(converted).filePath("linguists/cmn-pinyin/linguist.json"));
    expect(cmn.value("language").toString() == "cmn" && cmn.value("scheme").toString() == "pinyin",
           "the built linguist must declare its language and scheme");
    const auto cmnStages = references(cmn, "linguist/");
    expect(cmnStages == QStringList({"linguist/g2p=vendor/lang-cmn:inference/g2p",
                                     "linguist/onset=:inference/onset-cmn",
                                     "linguist/s2p=:inference/s2p-cmn"}),
           "the grapheme stage must come from the language package and the other two stages from "
           "the voicebank, got "
               + cmnStages.join(", "));
    // The rule tested by the shared engine fixture: the engine declares cmn but is not the binding
    // target.
    expect(!cmnStages.join(",").contains("vendor/engine"),
           "a shared engine wrapped by another G2P must not be the entry point of a language");

    const auto phonemes = cmn.value("exports").toObject().value("phonemes").toArray();
    QStringList spelled;
    for (const auto &value : phonemes) {
        spelled << value.toString();
    }
    // The inventory contains only the content phonemes. Reserved markers are excluded, because a
    // host never sends a marker through grapheme-to-phoneme conversion and must not be required to
    // support markers. `a` is included although it maps to itself, because it occurs inside `ma`
    // and `ba`; this occurrence distinguishes a syllable from a marker.
    expect(spelled == QStringList({"a", "b", "hum", "m"}),
           "the inventory must exclude the phonemes that the models mark as reserved and keep all "
           "others, got "
               + spelled.join(" "));
    expect(cmn.value("exports").toObject().value("openSet").toBool() == false,
           "a dictionary produces only its own entries, so the phoneme set must be closed");
    // AP is bare in the table, so it is reserved, and the singer declaration lists it where the
    // loader checks it again.
    QStringList reserved;
    for (const auto &value : singer.value("reservedPhonemes").toArray()) {
        reserved << value.toString();
    }
    expect(reserved == QStringList{"AP"},
           "the singer declaration must list the phonemes that the models mark as "
           "language-independent, got "
               + reserved.join(" "));
    // `hum` has the form of a marker, and no model contains it. It cannot be declared as reserved,
    // because the loader would reject the package. It must not be dropped either, because then no
    // diagnostic would report it as unusable. It remains in the inventory, and the converter
    // reports the reason.
    expect(diagnostics.contains("hum"),
           "a marker-shaped entry absent from every model must be reported; stderr: "
               + diagnostics.left(600));

    const auto s2p = read(QDir(converted).filePath("inferences/s2p-cmn/inference.json"));
    expect(s2p.value("variant").toString() == "dict", "the s2p mode must become the variant");
    expect(s2p.value("configuration").toObject().value("file").toString()
               == "../../assets/cmn.txt",
           "the s2p file path must be rewritten relative to the new declaration, and the file must "
           "not be moved");

    // English ships no stage of its own, so it binds to the complete linguist of the package.
    expect(languages.contains("eng"), "the language without its own stages must be bound");
    const auto engImports = references(singer, "lang/eng");
    expect(engImports == QStringList{"lang/eng=vendor/lang-eng:linguist/eng-arpabet"},
           "the language must be bound to the linguist of the language package, got "
               + engImports.join(", "));
    expect(!QFile::exists(QDir(converted).filePath("linguists/eng-arpabet/linguist.json")),
           "the converted package must not contain an English linguist");

    // No installed package supports the third language. The converter drops the language and
    // reports the drop instead of inventing a binding.
    expect(!languages.contains("xyz"), "an unsupported language must be dropped");
    expect(diagnostics.contains("xyz"),
           "the dropped language must be reported; stderr: " + diagnostics.left(600));

    // Both bindings reference other packages, so both packages must be declared as dependencies,
    // each at its compatVersion instead of its current version.
    QJsonObject dependencies;
    for (const auto &value : read(QDir(converted).filePath("desc.json"))
                                 .value("dependencies").toArray()) {
        dependencies.insert(value.toObject().value("id").toString(),
                            value.toObject().value("version"));
    }
    expect(dependencies.value("vendor/lang-cmn").toString() == "1.0.1.0",
           "the package of the grapheme stage must be a dependency at its compatVersion");
    expect(dependencies.value("vendor/lang-eng").toString() == "1.0.0.0",
           "the package of the linguist must be a dependency at its compatVersion");
    expect(!dependencies.contains("vendor/engine"),
           "the engine must not be a dependency: the converted package does not reference it, and "
           "the language packages declare it");

    // The --reserved option does not override the models either. Forcing the declaration would
    // build a package that does not load; therefore the converter rejects the phoneme and reports
    // the model that lacks it. The loader produces the same result, but only after publishing.
    const auto named = QDir(work.path()).filePath("named");
    QProcess again;
    again.start(QStringLiteral(TEST_PYTHON), {QStringLiteral(TEST_SCRIPT), source, "--output",
                                              named, "--packages", packages, "--reserved", "hum"});
    expect(again.waitForFinished(60000) && again.exitCode() == 0,
           "the converter must succeed with --reserved; stderr: "
               + QString::fromUtf8(again.readAllStandardError()).left(600));
    const auto insisted = QString::fromUtf8(again.readAllStandardError());
    QStringList stillReserved;
    for (const auto &value : read(QDir(named).filePath("characters/singer/config.json"))
                                 .value("reservedPhonemes").toArray()) {
        stillReserved << value.toString();
    }
    expect(stillReserved == QStringList{"AP"},
           "a phoneme absent from every model must not be declared, even if requested, got "
               + stillReserved.join(" "));
    expect(insisted.contains("acoustic"),
           "the model that lacks the phoneme must be reported; stderr: " + insisted.left(600));

    return failures == 0 ? 0 : 1;
}
