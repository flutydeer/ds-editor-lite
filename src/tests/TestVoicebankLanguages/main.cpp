// The converter's language half, on fixtures written here.
//
// The rest of the conversion is mechanical: a key is renamed or it is not. The languages are not,
// because where a language's three stages come from differs per language and the converter has to
// work it out from what is installed. Three rules decide it, and each one is a rule I got wrong
// before writing it down:
//
//   a shared engine is not a language's entry point, even though it declares languages
//   a voicebank that brought its own phoneme stage keeps it, rather than binding past it
//   a voicebank that brought nothing binds to the package's whole linguist, where there is one
//
// The fixtures are written here rather than kept in the tree so that what each rule is being shown
// is visible in the same file as the assertion about it.

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

    /// A shared engine plus three language packages, in the three shapes that actually ship.
    void writeLanguagePackages(const QString &root) {
        // The engine. It declares every language it can transcribe, which is exactly why it must
        // not be mistaken for any one language's entry point.
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

        // Mandarin: a chain over the engine, and nothing else. The phoneme stage of this language
        // is voicebank content, so the package deliberately has no linguist.
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

        // English: a chain over the same engine, and a whole linguist over that chain. Its
        // phonemes are the same for every voicebank, so the package can hold all of it.
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

    /// A 2.3 voicebank declaring three languages: one that brings its own phoneme stage, one that
    /// brings nothing, and one nothing installed can serve.
    void writeVoicebank(const QString &root) {
        // AP is a marker every host knows; hum is one this voicebank brings. Both sit among the
        // syllables and spell themselves, which is what a marker looks like in one of these.
        writeText(root + "/assets/cmn.txt", "AP\tAP\nhum\thum\nma\tm a\nba\tb a\na\ta\n");
        writeText(root + "/assets/cmn_onset.json",
                  "{\"phonemeTypes\":{\"m\":\"consonant\",\"b\":\"consonant\",\"a\":\"vowel\","
                  "\"AP\":\"vowel\"},\"rules\":[{\"pattern\":[\"vowel\"],\"onsets\":[0]}]}");
        write(root + "/inferences/vocoder/config.json", QJsonObject{
            {"$version", "1.0"}, {"id", "vocoder"}, {"class", "ai.svs.VocoderInference"},
            {"level", 1}, {"configuration", QJsonObject{{"model", "vocoder.onnx"}}},
        });
        // The phoneme table is where a voicebank says which of its phonemes belong to a language:
        // `cmn/a` does, bare `AP` does not. That is what makes AP a reserved phoneme and `a` not,
        // and it is read rather than assumed. `hum` is in no table at all, which is what makes it
        // neither reserved nor singable.
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
    expect(work.isValid(), "a temporary directory is needed");
    if (!work.isValid()) {
        return 1;
    }
    const auto packages = QDir(work.path()).filePath("packages");
    const auto source = QDir(work.path()).filePath("voicebank");
    const auto converted = QDir(work.path()).filePath("converted");
    writeLanguagePackages(packages);
    writeVoicebank(source);

    QProcess python;
    python.start("python3", {QStringLiteral(TEST_SCRIPT), source, "--output", converted,
                             "--packages", packages});
    expect(python.waitForFinished(60000), "the converter must finish");
    const auto diagnostics = QString::fromUtf8(python.readAllStandardError());
    expect(python.exitCode() == 0, "the converter must succeed, said: " + diagnostics.left(600));

    const auto singer = read(QDir(converted).filePath("characters/singer/config.json"));
    const auto languages = singer.value("configuration").toObject()
                               .value("languages").toObject();

    // Mandarin brought a dictionary and onset rules, so it keeps them: the linguist is built here
    // and only its grapheme stage reaches out to the package.
    expect(languages.contains("cmn"), "the language that brought its own stages is bound");
    const auto cmnImports = references(singer, "lang/cmn");
    expect(cmnImports == QStringList{"lang/cmn=:linguist/cmn-pinyin"},
           "and to a linguist in this package, got " + cmnImports.join(", "));
    const auto cmn = read(QDir(converted).filePath("linguists/cmn-pinyin/linguist.json"));
    expect(cmn.value("language").toString() == "cmn" && cmn.value("scheme").toString() == "pinyin",
           "the built linguist names its pair");
    const auto cmnStages = references(cmn, "linguist/");
    expect(cmnStages == QStringList({"linguist/g2p=vendor/lang-cmn:inference/g2p",
                                     "linguist/onset=:inference/onset-cmn",
                                     "linguist/s2p=:inference/s2p-cmn"}),
           "the grapheme stage is the package's and the other two are this voicebank's, got "
               + cmnStages.join(", "));
    // The rule the shared engine exists to test. It declares cmn, and it is still not the answer.
    expect(!cmnStages.join(",").contains("vendor/engine"),
           "a shared engine another G2P wraps is never a language's entry point");

    const auto phonemes = cmn.value("exports").toObject().value("phonemes").toArray();
    QStringList spelled;
    for (const auto &value : phonemes) {
        spelled << value.toString();
    }
    // The inventory is the *content* phonemes: reserved markers stay out of it, because a host
    // never sends a marker through grapheme-to-phoneme and must not be asked to have one. `a` is
    // in, although it spells itself, because it turns up inside `ma` and `ba` -- which is exactly
    // what tells a syllable from a marker.
    expect(spelled == QStringList({"a", "b", "hum", "m"}),
           "the inventory drops what the models mark reserved and keeps the rest, got "
               + spelled.join(" "));
    expect(cmn.value("exports").toObject().value("openSet").toBool() == false,
           "a dictionary can only produce what it holds, so the set is closed");
    // AP is bare in the table, so it is reserved, and the singer says so where the loader will
    // check it again.
    QStringList reserved;
    for (const auto &value : singer.value("configuration").toObject()
                                 .value("reservedPhonemes").toArray()) {
        reserved << value.toString();
    }
    expect(reserved == QStringList{"AP"},
           "the singer declares what the models mark language independent, got "
               + reserved.join(" "));
    // `hum` has a marker's shape and no model has it. It cannot be declared -- the loader would
    // refuse the package -- and it must not be dropped either, or nothing would ever say it is
    // unusable. It stays, and the converter says why.
    expect(diagnostics.contains("hum"),
           "a marker shaped entry no model has is pointed out, said: " + diagnostics.left(600));

    const auto s2p = read(QDir(converted).filePath("inferences/s2p-cmn/inference.json"));
    expect(s2p.value("variant").toString() == "dict", "the s2p mode becomes the variant");
    expect(s2p.value("configuration").toObject().value("file").toString()
               == "../../assets/cmn.txt",
           "and the file is re-expressed against the new declaration, not moved");

    // English brought nothing of its own, so it binds to the package's whole linguist.
    expect(languages.contains("eng"), "the language that brought nothing is bound too");
    const auto engImports = references(singer, "lang/eng");
    expect(engImports == QStringList{"lang/eng=vendor/lang-eng:linguist/eng-arpabet"},
           "and to the package's own linguist, got " + engImports.join(", "));
    expect(!QFile::exists(QDir(converted).filePath("linguists/eng-arpabet/linguist.json")),
           "nothing is built for it here");

    // The third has nothing installed to serve it. It is dropped and said so, not invented.
    expect(!languages.contains("xyz"), "a language nothing serves is dropped");
    expect(diagnostics.contains("xyz"), "and the drop is reported, said: " + diagnostics.left(600));

    // Both bindings reach out of this package, so both packages have to be declared, at the
    // version each says it is compatible back to rather than the version it happens to be.
    QJsonObject dependencies;
    for (const auto &value : read(QDir(converted).filePath("desc.json"))
                                 .value("dependencies").toArray()) {
        dependencies.insert(value.toObject().value("id").toString(),
                            value.toObject().value("version"));
    }
    expect(dependencies.value("vendor/lang-cmn").toString() == "1.0.1.0",
           "the grapheme stage's package is a dependency at its compatible version");
    expect(dependencies.value("vendor/lang-eng").toString() == "1.0.0.0",
           "and so is the linguist's");
    expect(!dependencies.contains("vendor/engine"),
           "the engine is not: nothing here names it, and its own dependents do");

    // Naming it by hand does not get past the models either. Insisting is how a package that
    // does not load gets built, so the converter refuses here instead and says which model is
    // missing it -- the same answer the loader would give, arrived at before publishing.
    const auto named = QDir(work.path()).filePath("named");
    QProcess again;
    again.start("python3", {QStringLiteral(TEST_SCRIPT), source, "--output", named,
                            "--packages", packages, "--reserved", "hum"});
    expect(again.waitForFinished(60000) && again.exitCode() == 0,
           "the converter must still succeed, said: "
               + QString::fromUtf8(again.readAllStandardError()).left(600));
    const auto insisted = QString::fromUtf8(again.readAllStandardError());
    QStringList stillReserved;
    for (const auto &value : read(QDir(named).filePath("characters/singer/config.json"))
                                 .value("configuration").toObject()
                                 .value("reservedPhonemes").toArray()) {
        stillReserved << value.toString();
    }
    expect(stillReserved == QStringList{"AP"},
           "a phoneme no model has is not declared however it is asked for, got "
               + stillReserved.join(" "));
    expect(insisted.contains("acoustic"),
           "and the model that lacks it is named, said: " + insisted.left(600));

    return failures == 0 ? 0 : 1;
}
