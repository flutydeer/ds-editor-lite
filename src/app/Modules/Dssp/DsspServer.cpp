#include "DsspServer.h"

#include "DsspApi.h"
#include "DsspExtraction.h"
#include "DsspLanguage.h"
#include "DsspMetadata.h"
#include "DsspSynthesis.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>

#include <chrono>

// Single translation unit owning the embedded HTTP server implementation.
// A generous payload limit accommodates base64 data URLs on extraction input.
#define CPPHTTPLIB_PAYLOAD_MAX_LENGTH ((size_t)1 << 28)
#include "3rdparty/httplib/httplib.h"

Q_LOGGING_CATEGORY(logDsspServer, "dssp.server")

namespace {

    using DsspApi::Result;

    QString decodePathSegment(const std::string &segment) {
        return QString::fromUtf8(
            httplib::detail::decode_url(segment, /*convert_plus_to_space=*/false));
    }

    void writeResult(const Result &result, httplib::Response &res) {
        res.status = result.status;
        QJsonDocument doc;
        if (result.body.isObject())
            doc.setObject(result.body.toObject());
        else if (result.body.isArray())
            doc.setArray(result.body.toArray());
        res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(),
                        result.contentType.toUtf8().toStdString());
    }

    Result parseJsonBody(const httplib::Request &req, QJsonObject &body) {
        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(
            QByteArray::fromRawData(req.body.data(), static_cast<qsizetype>(req.body.size())),
            &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return Result::fail(DsspApi::validationError(
                QStringLiteral("Invalid request body: %1").arg(parseError.errorString())));
        }
        body = doc.object();
        return Result::ok(body);
    }

} // namespace

DsspServer::DsspServer(QObject *parent) : QObject(parent) {
}

DsspServer::~DsspServer() {
    stop();
}

void DsspServer::registerRoutes() {
    using namespace httplib;

    // === Application ===
    m_server->Get("/v1/info", [](const Request &, Response &res) {
        writeResult(DsspMetadata::applicationInfo(), res);
    });

    // === Architecture and singer metadata ===
    m_server->Get("/v1/arch", [](const Request &, Response &res) {
        writeResult(DsspMetadata::architectureList(), res);
    });
    m_server->Get(R"(/v1/arch/([^/]+))", [](const Request &req, Response &res) {
        writeResult(DsspMetadata::architecture(decodePathSegment(req.matches[1])), res);
    });
    m_server->Get("/v1/singer", [](const Request &, Response &res) {
        writeResult(DsspMetadata::singerList(), res);
    });
    m_server->Get(R"(/v1/arch/([^/]+)/singer)", [](const Request &req, Response &res) {
        writeResult(DsspMetadata::archSingerList(decodePathSegment(req.matches[1])), res);
    });
    m_server->Get(R"(/v1/arch/([^/]+)/singer/([^/]+))", [](const Request &req, Response &res) {
        writeResult(DsspMetadata::singer(decodePathSegment(req.matches[2])), res);
    });
    m_server->Get(R"(/v1/arch/([^/]+)/singer/([^/]+)/avatar)", [](const Request &req, Response &res) {
        writeResult(DsspMetadata::singerAvatar(decodePathSegment(req.matches[2])), res);
    });
    m_server->Get(R"(/v1/arch/([^/]+)/singer/([^/]+)/background)",
                  [](const Request &req, Response &res) {
                      writeResult(DsspMetadata::singerBackground(decodePathSegment(req.matches[2])),
                                  res);
                  });
    m_server->Get(R"(/v1/arch/([^/]+)/singer/([^/]+)/demo_audio)",
                  [](const Request &req, Response &res) {
                      writeResult(DsspMetadata::singerDemoAudioList(decodePathSegment(req.matches[2])),
                                  res);
                  });

    // === Synthesis ===
    m_server->Post("/v1/env_tag", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspMetadata::envTag(body), res);
    });
    m_server->Post("/v1/synth/pronunciation", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspLanguage::pronunciation(body), res);
    });
    m_server->Post("/v1/synth/phoneme", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspLanguage::phoneme(body), res);
    });
    m_server->Post("/v1/synth/duration", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspSynthesis::duration(body), res);
    });
    m_server->Post("/v1/synth/parameter", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspSynthesis::parameter(body), res);
    });
    m_server->Post("/v1/synth/audio", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspSynthesis::audio(body), res);
    });

    // === Extraction ===
    m_server->Get("/v1/extractor", [](const Request &, Response &res) {
        writeResult(DsspExtraction::extractorList(), res);
    });
    m_server->Get(R"(/v1/extractor/note/([^/]+))", [](const Request &req, Response &res) {
        writeResult(DsspExtraction::noteExtractor(decodePathSegment(req.matches[1])), res);
    });
    m_server->Get(R"(/v1/extractor/tempo/([^/]+))", [](const Request &req, Response &res) {
        writeResult(DsspExtraction::tempoExtractor(decodePathSegment(req.matches[1])), res);
    });
    m_server->Get(R"(/v1/extractor/pitch/([^/]+))", [](const Request &req, Response &res) {
        writeResult(DsspExtraction::pitchExtractor(decodePathSegment(req.matches[1])), res);
    });
    m_server->Post("/v1/extract/note", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspExtraction::extractNote(body), res);
    });
    m_server->Post("/v1/extract/tempo", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspExtraction::extractTempo(body), res);
    });
    m_server->Post("/v1/extract/pitch", [](const Request &req, Response &res) {
        QJsonObject body;
        auto parsed = parseJsonBody(req, body);
        if (parsed.isProblem) {
            writeResult(parsed, res);
            return;
        }
        writeResult(DsspExtraction::extractPitch(body), res);
    });
}

bool DsspServer::start(const QString &host, int port, QString *errorMessage) {
    if (m_running)
        return true;

    m_server = std::make_unique<httplib::Server>();
    m_server->set_read_timeout(120, 0);
    m_server->set_write_timeout(120, 0);
    registerRoutes();

    const auto hostUtf8 = host.toUtf8().toStdString();
    QString listenError;
    m_listenThread = std::thread([this, hostUtf8, port, &listenError] {
        try {
            if (!m_server->listen(hostUtf8.c_str(), port))
                listenError = QStringLiteral("Failed to bind %1:%2").arg(
                    QString::fromStdString(hostUtf8), QString::number(port));
        } catch (const std::exception &e) {
            listenError = QStringLiteral("HTTP server exception: %1")
                              .arg(QString::fromUtf8(e.what()));
        }
    });

    // Wait for a successful bind (is_running) or an early listen failure.
    for (int i = 0; i < 500; ++i) {
        if (m_server->is_running()) {
            m_running = true;
            qCInfo(logDsspServer).noquote()
                << "DSSP service listening on" << host << "port" << port;
            return true;
        }
        if (!listenError.isEmpty())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (m_listenThread.joinable())
        m_listenThread.join();
    m_server.reset();
    if (errorMessage)
        *errorMessage = listenError.isEmpty()
                            ? QStringLiteral("Failed to start HTTP server on %1:%2")
                                  .arg(host, QString::number(port))
                            : listenError;
    qCWarning(logDsspServer).noquote() << "DSSP service failed to start:" << *errorMessage;
    return false;
}

void DsspServer::stop() {
    if (!m_running.exchange(false))
        return;
    if (m_server)
        m_server->stop();
    if (m_listenThread.joinable())
        m_listenThread.join();
    // Resetting joins the thread pool, waiting for in-flight handlers (e.g. a
    // running synthesis) to finish before the inference environment is torn down.
    m_server.reset();
    qCInfo(logDsspServer) << "DSSP service stopped";
}

bool DsspServer::isRunning() const {
    return m_running;
}
