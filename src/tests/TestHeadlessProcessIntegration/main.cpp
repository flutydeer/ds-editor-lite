#include <Bootstrap/SingleInstanceCoordinator.h>
#include <Bootstrap/SingleInstanceIdentity.h>
#include <BootstrapWatcher.h>
#include <UpstreamMcpClient.h>

#include <lite/AutomationWire/McpProtocol.h>
#include <lite/ProductMetadata.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <array>
#include <cerrno>
#include <functional>
#include <optional>
#include <utility>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#  include <shellapi.h>
#  include <tlhelp32.h>
#  include <winternl.h>
#else
#  include <signal.h>
#  include <unistd.h>
#endif

namespace {
    bool fail(const QString &message) {
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    bool waitUntil(const std::function<bool()> &predicate, const int timeoutMilliseconds) {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < timeoutMilliseconds) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(10);
        }
        return predicate();
    }

    void stopProcess(QProcess &process) {
        if (process.state() == QProcess::NotRunning)
            return;
        process.terminate();
        if (!process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished(5000);
        }
    }

    bool writeWaveFixture(const QString &path) {
        constexpr quint32 sampleRate = 8000;
        constexpr quint16 channels = 1;
        constexpr quint16 bitsPerSample = 16;
        constexpr quint32 sampleCount = 800;
        const QByteArray samples(
            static_cast<qsizetype>(sampleCount * channels * (bitsPerSample / 8)), '\0');
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + samples.size());
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << channels << sampleRate
               << quint32(sampleRate * channels * (bitsPerSample / 8))
               << quint16(channels * (bitsPerSample / 8)) << bitsPerSample;
        stream.writeRawData("data", 4);
        stream << quint32(samples.size());
        stream.writeRawData(samples.constData(), samples.size());
        return stream.status() == QDataStream::Ok;
    }

    QString compactJson(const QJsonObject &object) {
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    QJsonObject nativeRequest(const QString &id, const QString &method,
                              const std::optional<QJsonObject> &params = std::nullopt) {
        QJsonObject request{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"),      id                   },
            {QStringLiteral("method"),  method               },
        };
        if (params)
            request.insert(QStringLiteral("params"), *params);
        return request;
    }

    std::optional<QJsonObject> nativeExchange(QNetworkAccessManager &manager, const QUrl &endpoint,
                                              const QJsonObject &request, QString &error,
                                              const int timeoutMilliseconds = 5000) {
        error.clear();
        QNetworkRequest httpRequest(endpoint);
        httpRequest.setRawHeader("Content-Type", "application/json; charset=utf-8");
        httpRequest.setRawHeader("Accept", "application/json");
        auto *reply =
            manager.post(httpRequest, QJsonDocument(request).toJson(QJsonDocument::Compact));

        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(timeoutMilliseconds);
        loop.exec();

        if (!reply->isFinished()) {
            reply->abort();
            error = QStringLiteral("Native request timed out");
            reply->deleteLater();
            return std::nullopt;
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        const auto responseBytes = reply->readAll();
        const auto networkErrorText = reply->errorString();
        reply->deleteLater();
        if (status != 200 || networkError != QNetworkReply::NoError) {
            error =
                QStringLiteral("Native HTTP request failed: status=%1, network_error=%2, body=%3")
                    .arg(status)
                    .arg(networkErrorText, QString::fromUtf8(responseBytes));
            return std::nullopt;
        }

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(responseBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("Native response was not a JSON object: %1")
                        .arg(QString::fromUtf8(responseBytes));
            return std::nullopt;
        }
        const auto response = document.object();
        if (response.value(QStringLiteral("id")) != request.value(QStringLiteral("id"))) {
            error = QStringLiteral("Native response returned an unexpected id: %1")
                        .arg(compactJson(response));
            return std::nullopt;
        }
        return response;
    }

    std::optional<QJsonObject> mcpExchange(DsConnector::UpstreamMcpClient &client,
                                           const QString &method, QJsonObject params,
                                           QString &error, const int timeoutMilliseconds = 5000) {
        error.clear();
        std::optional<DsConnector::UpstreamResult> response;
        client.send(
            method, std::move(params),
            [&response](DsConnector::UpstreamResult result) { response = std::move(result); },
            timeoutMilliseconds);
        if (!waitUntil([&response] { return response.has_value(); }, timeoutMilliseconds + 1000)) {
            error = QStringLiteral("MCP request timed out");
            return std::nullopt;
        }
        if (!response->connectorError.isEmpty()) {
            error = QStringLiteral("MCP transport error: %1 (%2)")
                        .arg(response->connectorError, response->connectorErrorMessage);
            return std::nullopt;
        }
        if (response->protocolError) {
            error = QStringLiteral("MCP protocol error %1: %2")
                        .arg(response->protocolError->code)
                        .arg(response->protocolError->message);
            return std::nullopt;
        }
        return response->result;
    }

    bool tcpListenerAvailable(const quint16 port, const int timeoutMilliseconds = 250) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress(QStringLiteral("127.0.0.1")), port);
        const auto connected = socket.waitForConnected(timeoutMilliseconds);
        socket.abort();
        return connected;
    }

    bool localServiceAvailable(const QString &serviceName, const int timeoutMilliseconds = 250) {
        QLocalSocket socket;
        socket.connectToServer(serviceName, QIODevice::ReadWrite);
        const auto connected = socket.waitForConnected(timeoutMilliseconds);
        socket.abort();
        return connected;
    }

#ifdef Q_OS_WIN
    struct ProcessSnapshot {
        QString executablePath;
        QString currentDirectory;
        QStringList arguments;
    };

    struct RemoteProcessParametersPrefix {
        ULONG maximumLength;
        ULONG length;
        ULONG flags;
        ULONG debugFlags;
        HANDLE consoleHandle;
        ULONG consoleFlags;
        HANDLE standardInput;
        HANDLE standardOutput;
        HANDLE standardError;
        UNICODE_STRING currentDirectoryPath;
        HANDLE currentDirectoryHandle;
        UNICODE_STRING dllPath;
        UNICODE_STRING imagePathName;
        UNICODE_STRING commandLine;
    };

    static_assert(offsetof(RemoteProcessParametersPrefix, imagePathName) ==
                  offsetof(RTL_USER_PROCESS_PARAMETERS, ImagePathName));
    static_assert(offsetof(RemoteProcessParametersPrefix, commandLine) ==
                  offsetof(RTL_USER_PROCESS_PARAMETERS, CommandLine));

    QString normalizedPath(const QString &path) {
        return QDir::toNativeSeparators(QDir::cleanPath(QFileInfo(path).absoluteFilePath()))
            .toCaseFolded();
    }

    std::optional<QString> readRemoteString(HANDLE process, const UNICODE_STRING &value) {
        if (value.Length == 0)
            return QString{};
        if (!value.Buffer || value.Length > value.MaximumLength ||
            value.Length % sizeof(wchar_t) != 0) {
            return std::nullopt;
        }
        QByteArray bytes(value.Length, '\0');
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(process, value.Buffer, bytes.data(), bytes.size(), &bytesRead) ||
            bytesRead != static_cast<SIZE_T>(bytes.size())) {
            return std::nullopt;
        }
        return QString::fromWCharArray(reinterpret_cast<const wchar_t *>(bytes.constData()),
                                       value.Length / sizeof(wchar_t));
    }

    std::optional<ProcessSnapshot> processSnapshot(const qint64 processId) {
        const auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE,
                                         FALSE, static_cast<DWORD>(processId));
        if (!process)
            return std::nullopt;
        const auto closeProcess = qScopeGuard([process] { CloseHandle(process); });

        using NtQueryInformationProcessFunction =
            NTSTATUS(NTAPI *)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
        const auto ntdll = GetModuleHandleW(L"ntdll.dll");
        const auto query = reinterpret_cast<NtQueryInformationProcessFunction>(
            ntdll ? GetProcAddress(ntdll, "NtQueryInformationProcess") : nullptr);
        if (!query)
            return std::nullopt;
        PROCESS_BASIC_INFORMATION basic{};
        if (query(process, ProcessBasicInformation, &basic, sizeof(basic), nullptr) < 0 ||
            !basic.PebBaseAddress) {
            return std::nullopt;
        }
        PEB peb{};
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(process, basic.PebBaseAddress, &peb, sizeof(peb), &bytesRead) ||
            bytesRead != sizeof(peb) || !peb.ProcessParameters) {
            return std::nullopt;
        }
        RemoteProcessParametersPrefix parameters{};
        if (!ReadProcessMemory(process, peb.ProcessParameters, &parameters, sizeof(parameters),
                               &bytesRead) ||
            bytesRead != sizeof(parameters)) {
            return std::nullopt;
        }
        const auto currentDirectory = readRemoteString(process, parameters.currentDirectoryPath);
        const auto commandLine = readRemoteString(process, parameters.commandLine);
        if (!currentDirectory || !commandLine)
            return std::nullopt;

        std::wstring executableBuffer(32768, L'\0');
        DWORD executableLength = static_cast<DWORD>(executableBuffer.size());
        if (!QueryFullProcessImageNameW(process, 0, executableBuffer.data(), &executableLength))
            return std::nullopt;
        executableBuffer.resize(executableLength);

        int argumentCount = 0;
        auto *arguments =
            CommandLineToArgvW(reinterpret_cast<LPCWSTR>(commandLine->utf16()), &argumentCount);
        if (!arguments)
            return std::nullopt;
        const auto freeArguments = qScopeGuard([arguments] { LocalFree(arguments); });
        QStringList parsedArguments;
        parsedArguments.reserve(argumentCount);
        for (int index = 0; index < argumentCount; ++index)
            parsedArguments.append(QString::fromWCharArray(arguments[index]));
        return ProcessSnapshot{
            .executablePath = QString::fromStdWString(executableBuffer),
            .currentDirectory = *currentDirectory,
            .arguments = std::move(parsedArguments),
        };
    }

    bool processMatches(const ProcessSnapshot &snapshot, const QString &executablePath,
                        const QString &workingDirectory, const QStringList &arguments) {
        return normalizedPath(snapshot.executablePath) == normalizedPath(executablePath) &&
               normalizedPath(snapshot.currentDirectory) == normalizedPath(workingDirectory) &&
               snapshot.arguments.mid(1) == arguments;
    }

    bool processIsRunning(const qint64 processId) {
        const auto process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
        if (!process)
            return false;
        const auto running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
        CloseHandle(process);
        return running;
    }

    qint64 findOwnedProcess(const QString &executablePath, const QString &workingDirectory,
                            const QStringList &arguments, const qint64 excludedProcessId) {
        const auto processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (processes == INVALID_HANDLE_VALUE)
            return 0;
        const auto closeProcesses = qScopeGuard([processes] { CloseHandle(processes); });
        PROCESSENTRY32W entry{.dwSize = sizeof(PROCESSENTRY32W)};
        if (!Process32FirstW(processes, &entry))
            return 0;
        do {
            if (entry.th32ProcessID == 0 ||
                entry.th32ProcessID == static_cast<DWORD>(excludedProcessId)) {
                continue;
            }
            const auto snapshot = processSnapshot(entry.th32ProcessID);
            if (snapshot && processMatches(*snapshot, executablePath, workingDirectory, arguments))
                return entry.th32ProcessID;
        } while (Process32NextW(processes, &entry));
        return 0;
    }

    void terminateOwnedProcess(const qint64 processId, const QString &executablePath,
                               const QString &workingDirectory, const QStringList &arguments) {
        const auto snapshot = processSnapshot(processId);
        if (!snapshot || !processMatches(*snapshot, executablePath, workingDirectory, arguments))
            return;
        const auto process =
            OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
        if (!process)
            return;
        TerminateProcess(process, EXIT_FAILURE);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
    }

    struct WindowProbe {
        DWORD processId = 0;
        int count = 0;
    };

    BOOL CALLBACK countProcessWindow(HWND window, LPARAM parameter) {
        auto &probe = *reinterpret_cast<WindowProbe *>(parameter);
        DWORD processId = 0;
        GetWindowThreadProcessId(window, &processId);
        if (processId == probe.processId)
            ++probe.count;
        return TRUE;
    }

    int topLevelWindowCount(const qint64 processId) {
        WindowProbe probe{static_cast<DWORD>(processId), 0};
        if (!EnumWindows(countProcessWindow, reinterpret_cast<LPARAM>(&probe)))
            return -1;
        return probe.count;
    }

    bool sendConsoleControlEvent(const qint64 processId, const DWORD eventType, QString &error) {
        DWORD consoleProcesses[2];
        const auto hadConsole = GetConsoleProcessList(consoleProcesses, 2) != 0;
        if (hadConsole && !FreeConsole()) {
            error = QStringLiteral("Could not detach the test console: Windows error %1")
                        .arg(GetLastError());
            return false;
        }

        if (!AttachConsole(static_cast<DWORD>(processId))) {
            const auto errorNumber = GetLastError();
            if (hadConsole)
                AttachConsole(ATTACH_PARENT_PROCESS);
            error = QStringLiteral("Could not attach the child console: Windows error %1")
                        .arg(errorNumber);
            return false;
        }

        const auto sent = GenerateConsoleCtrlEvent(eventType, static_cast<DWORD>(processId));
        const auto sendError = sent ? ERROR_SUCCESS : GetLastError();
        const auto detached = FreeConsole();
        const auto detachError = detached ? ERROR_SUCCESS : GetLastError();
        const auto restored = !hadConsole || AttachConsole(ATTACH_PARENT_PROCESS);
        const auto restoreError = restored ? ERROR_SUCCESS : GetLastError();
        if (!sent || !detached || !restored) {
            error = QStringLiteral("Could not deliver CTRL_BREAK_EVENT: send=%1, detach=%2, "
                                   "restore=%3")
                        .arg(sendError)
                        .arg(detachError)
                        .arg(restoreError);
            return false;
        }
        return true;
    }
#else
    struct ProcessSnapshot {
        QString executablePath;
        QString currentDirectory;
        QStringList arguments;
    };

    std::optional<ProcessSnapshot> processSnapshot(qint64) {
        return std::nullopt;
    }

    bool processMatches(const ProcessSnapshot &, const QString &, const QString &,
                        const QStringList &) {
        return true;
    }

    bool processIsRunning(qint64) {
        return false;
    }

    qint64 findOwnedProcess(const QString &, const QString &, const QStringList &, qint64) {
        return 0;
    }

    void terminateOwnedProcess(qint64, const QString &, const QString &, const QStringList &) {
    }

    int topLevelWindowCount(qint64) {
        return 0;
    }
#endif

    QString processDiagnostics(QProcess &process, const QString &appDataRoot, const quint16 port) {
        return QStringLiteral("appdata=%1; port=%2; state=%3; exit_status=%4; exit_code=%5; "
                              "stdout=%6; stderr=%7")
            .arg(appDataRoot)
            .arg(port)
            .arg(static_cast<int>(process.state()))
            .arg(static_cast<int>(process.exitStatus()))
            .arg(process.exitCode())
            .arg(QString::fromUtf8(process.readAllStandardOutput()),
                 QString::fromUtf8(process.readAllStandardError()));
    }

    bool runIntegration(const QString &editorPath, const QString &platformPluginDirectory) {
        QTemporaryDir isolatedRoot;
        if (!isolatedRoot.isValid())
            return fail(QStringLiteral("Could not create an isolated headless-test data root"));

        const auto appDataRoot = QDir(isolatedRoot.path()).filePath(QStringLiteral("Roaming"));
        const auto localDataRoot = QDir(isolatedRoot.path()).filePath(QStringLiteral("Local"));
        if (!QDir().mkpath(appDataRoot) || !QDir().mkpath(localDataRoot)) {
            return fail(QStringLiteral("Could not create isolated application-data roots"));
        }
        const auto editorDataDirectory =
            QDir(appDataRoot)
                .filePath(QStringLiteral("%1/%2").arg(
                    QString::fromLatin1(LiteProductMetadata::Publisher),
                    QString::fromLatin1(LiteProductMetadata::ProductName)));
        if (!QDir().mkpath(editorDataDirectory))
            return fail(QStringLiteral("Could not create the isolated editor data directory"));

        QFile config(QDir(editorDataDirectory).filePath(QStringLiteral("appConfig.json")));
        if (!config.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return fail(QStringLiteral("Could not seed the isolated editor configuration"));
        config.write(
            QJsonDocument(QJsonObject{
                              {QStringLiteral("automation"),
                               QJsonObject{
                                   {QStringLiteral("accessRoots"),
                                    QJsonArray{QDir::fromNativeSeparators(isolatedRoot.path())}},
                               }},
        })
                .toJson(QJsonDocument::Compact));
        config.close();
        const auto audioPath = isolatedRoot.filePath(QStringLiteral("headless-import.wav"));
        if (!writeWaveFixture(audioPath))
            return fail(QStringLiteral("Could not create the isolated audio fixture"));

        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("APPDATA"), appDataRoot);
        environment.insert(QStringLiteral("LOCALAPPDATA"), localDataRoot);
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"),
                           QStringLiteral("phase3-deliberately-invalid-platform"));
        environment.insert(QStringLiteral("QT_LOGGING_TO_CONSOLE"), QStringLiteral("1"));

        QTcpServer portProbe;
        if (!portProbe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0)) {
            return fail(QStringLiteral("Could not allocate a headless control port: %1")
                            .arg(portProbe.errorString()));
        }
        const auto controlPort = portProbe.serverPort();
        portProbe.close();
        const QUrl nativeEndpoint(
            QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(controlPort));
        const auto serviceName = SingleInstanceIdentity::serviceName(editorDataDirectory);

        QProcess editor;
        QProcess signalEditor;
        QProcess conflictingEditor;
        QProcess mcpEditor;
        QProcess competitionHeadless;
        QProcess guiSecondary;
        qint64 restartedProcessId = 0;
        qint64 restartSourceProcessId = 0;
        QString restartWorkingDirectory;
        QStringList restartArguments;
        const auto cleanup = qScopeGuard([&] {
            stopProcess(guiSecondary);
            stopProcess(competitionHeadless);
            stopProcess(mcpEditor);
            stopProcess(conflictingEditor);
            stopProcess(signalEditor);
            stopProcess(editor);
            if (restartedProcessId == 0 && !restartWorkingDirectory.isEmpty()) {
                restartedProcessId = findOwnedProcess(editorPath, restartWorkingDirectory,
                                                      restartArguments, restartSourceProcessId);
            }
            if (restartedProcessId != 0) {
                terminateOwnedProcess(restartedProcessId, editorPath, restartWorkingDirectory,
                                      restartArguments);
            }
        });
        editor.setProcessEnvironment(environment);
        editor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        editor.setProcessChannelMode(QProcess::SeparateChannels);
        editor.start(editorPath, {QStringLiteral("--headless"), QStringLiteral("--no-mcp"),
                                  QStringLiteral("--control-level"), QStringLiteral("l3"),
                                  QStringLiteral("--control-port"), QString::number(controlPort)});
        if (!editor.waitForStarted(10000)) {
            return fail(
                QStringLiteral("Headless editor failed to start: %1").arg(editor.errorString()));
        }

        DsConnector::BootstrapWatcher watcher(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                              QStringLiteral("1"), serviceName);
        watcher.start();
        const auto watcherCleanup = qScopeGuard([&watcher] { watcher.stop(); });

        QNetworkAccessManager manager;
        manager.setProxy(QNetworkProxy::NoProxy);
        QString exchangeError;
        std::optional<QJsonObject> statusResponse;
        const auto nativeSettled = waitUntil(
            [&] {
                if (editor.state() == QProcess::NotRunning)
                    return true;
                const auto response =
                    nativeExchange(manager, nativeEndpoint,
                                   nativeRequest(QStringLiteral("status"),
                                                 QStringLiteral("application.get_status")),
                                   exchangeError, 500);
                if (!response || !response->value(QStringLiteral("result")).isObject())
                    return false;
                statusResponse = response;
                return true;
            },
            45000);
        if (!nativeSettled || !statusResponse) {
            return fail(
                QStringLiteral("Headless Native endpoint did not become ready: %1; %2")
                    .arg(exchangeError, processDiagnostics(editor, appDataRoot, controlPort)));
        }

        const auto status = statusResponse->value(QStringLiteral("result")).toObject();
        const auto documents = status.value(QStringLiteral("documents")).toArray();
        if (status.value(QStringLiteral("host_mode")) != QStringLiteral("headless") ||
            status.value(QStringLiteral("control_level")) != QStringLiteral("l3") ||
            documents.size() != 1 || !status.value(QStringLiteral("windows")).toArray().isEmpty()) {
            return fail(
                QStringLiteral("Headless status did not expose one document and no windows: %1")
                    .arg(compactJson(status)));
        }
        const auto document = documents.first().toObject();
        const auto documentId = document.value(QStringLiteral("document_id")).toString();
        const auto initialRevision = document.value(QStringLiteral("revision")).toInteger(-1);
        if (documentId.isEmpty() || initialRevision < 0) {
            return fail(QStringLiteral("Headless status did not expose a real document version: %1")
                            .arg(compactJson(status)));
        }

        const auto themeSettings = nativeExchange(
            manager, nativeEndpoint,
            nativeRequest(QStringLiteral("theme-settings"), QStringLiteral("settings.query"),
                          QJsonObject{
                              {QStringLiteral("domains"), QJsonArray{QStringLiteral("theme")}}
        }),
            exchangeError);
        const auto themeDomain = themeSettings ? themeSettings->value(QStringLiteral("result"))
                                                     .toObject()
                                                     .value(QStringLiteral("domains"))
                                                     .toObject()
                                                     .value(QStringLiteral("theme"))
                                                     .toObject()
                                               : QJsonObject{};
        if (!themeSettings || themeSettings->contains(QStringLiteral("error")) ||
            themeDomain.value(QStringLiteral("available")).toBool(true) ||
            themeDomain.value(QStringLiteral("unavailable_reason")).toString().isEmpty()) {
            return fail(
                QStringLiteral("Headless theme settings did not report UI unavailability: %1")
                    .arg(themeSettings ? compactJson(*themeSettings) : exchangeError));
        }

        const auto bootstrapSettled = waitUntil(
            [&] {
                return (watcher.observation().snapshot &&
                        watcher.observation().snapshot->result.state ==
                            SingleInstanceAutomationState::ServerDisabled) ||
                       editor.state() == QProcess::NotRunning;
            },
            10000);
        if (!bootstrapSettled || !watcher.observation().snapshot) {
            return fail(
                QStringLiteral("Headless bootstrap snapshot did not become available: %1; %2")
                    .arg(watcher.observation().error,
                         processDiagnostics(editor, appDataRoot, controlPort)));
        }
        const auto &bootstrap = watcher.observation().snapshot->result;
        if (bootstrap.hostMode != QStringLiteral("headless") || bootstrap.serverEnabled ||
            !bootstrap.serverEndpoint.isEmpty() ||
            bootstrap.state != SingleInstanceAutomationState::ServerDisabled ||
            bootstrap.editorInstanceId !=
                status.value(QStringLiteral("editor_instance_id")).toString()) {
            return fail(QStringLiteral("Headless --no-mcp bootstrap state was incorrect"));
        }

        const auto windowCount = topLevelWindowCount(editor.processId());
        if (windowCount != 0) {
            return fail(QStringLiteral("Headless process exposed %1 top-level windows; %2")
                            .arg(windowCount)
                            .arg(processDiagnostics(editor, appDataRoot, controlPort)));
        }

        const auto getDocument = [&](const QString &requestId) {
            return nativeExchange(manager, nativeEndpoint,
                                  nativeRequest(requestId, QStringLiteral("documents.get"),
                                                QJsonObject{
                                                    {QStringLiteral("document_id"), documentId}
            }),
                                  exchangeError);
        };
        const auto initialDocument = getDocument(QStringLiteral("document-before"));
        if (!initialDocument || initialDocument->contains(QStringLiteral("error"))) {
            return fail(QStringLiteral("Initial documents.get failed: %1")
                            .arg(initialDocument ? compactJson(*initialDocument) : exchangeError));
        }
        const auto initialTrackCount = initialDocument->value(QStringLiteral("result"))
                                           .toObject()
                                           .value(QStringLiteral("snapshot"))
                                           .toObject()
                                           .value(QStringLiteral("statistics"))
                                           .toObject()
                                           .value(QStringLiteral("track_count"))
                                           .toInteger(-1);

        const auto insertTrack = [&](const QString &requestId, const qint64 expectedRevision,
                                     const QString &clientRef) {
            return nativeExchange(
                manager, nativeEndpoint,
                nativeRequest(requestId, QStringLiteral("tracks.insert"),
                              QJsonObject{
                                  {QStringLiteral("document_id"),       documentId      },
                                  {QStringLiteral("expected_revision"), expectedRevision},
                                  {QStringLiteral("index"),             0               },
                                  {QStringLiteral("tracks"),
                                   QJsonArray{QJsonObject{
                                       {QStringLiteral("client_ref"), clientRef},
                                       {QStringLiteral("name"), QStringLiteral("Headless Track")},
                                       {QStringLiteral("color_index"), 0},
                                   }}                                                   },
            }),
                exchangeError);
        };
        const auto insertion =
            insertTrack(QStringLiteral("insert"), initialRevision, QStringLiteral("insert-one"));
        const auto insertionResult =
            insertion ? insertion->value(QStringLiteral("result")).toObject() : QJsonObject{};
        const auto insertedRevision = insertionResult.value(QStringLiteral("current"))
                                          .toObject()
                                          .value(QStringLiteral("revision"))
                                          .toInteger(-1);
        if (!insertion || insertion->contains(QStringLiteral("error")) ||
            !insertionResult.value(QStringLiteral("changed")).toBool() ||
            insertedRevision != initialRevision + 1) {
            return fail(QStringLiteral("Native tracks.insert failed: %1")
                            .arg(insertion ? compactJson(*insertion) : exchangeError));
        }

        const auto undo = nativeExchange(
            manager, nativeEndpoint,
            nativeRequest(QStringLiteral("undo"), QStringLiteral("history.undo"),
                          QJsonObject{
                              {QStringLiteral("document_id"),       documentId      },
                              {QStringLiteral("expected_revision"), insertedRevision},
        }),
            exchangeError);
        const auto undoResult =
            undo ? undo->value(QStringLiteral("result")).toObject() : QJsonObject{};
        const auto undoneRevision = undoResult.value(QStringLiteral("current"))
                                        .toObject()
                                        .value(QStringLiteral("revision"))
                                        .toInteger(-1);
        if (!undo || undo->contains(QStringLiteral("error")) ||
            !undoResult.value(QStringLiteral("changed")).toBool() ||
            undoneRevision != insertedRevision + 1) {
            return fail(QStringLiteral("Native history.undo failed: %1")
                            .arg(undo ? compactJson(*undo) : exchangeError));
        }
        const auto documentAfterUndo = getDocument(QStringLiteral("document-after-undo"));
        const auto trackCountAfterUndo = documentAfterUndo
                                             ? documentAfterUndo->value(QStringLiteral("result"))
                                                   .toObject()
                                                   .value(QStringLiteral("snapshot"))
                                                   .toObject()
                                                   .value(QStringLiteral("statistics"))
                                                   .toObject()
                                                   .value(QStringLiteral("track_count"))
                                                   .toInteger(-1)
                                             : -1;
        if (!documentAfterUndo || documentAfterUndo->contains(QStringLiteral("error")) ||
            trackCountAfterUndo != initialTrackCount) {
            return fail(
                QStringLiteral("Undo did not restore the document track count: %1")
                    .arg(documentAfterUndo ? compactJson(*documentAfterUndo) : exchangeError));
        }

        const auto redo =
            nativeExchange(manager, nativeEndpoint,
                           nativeRequest(QStringLiteral("redo"), QStringLiteral("history.redo"),
                                         QJsonObject{
                                             {QStringLiteral("document_id"),       documentId    },
                                             {QStringLiteral("expected_revision"), undoneRevision},
        }),
                           exchangeError);
        const auto redoResult =
            redo ? redo->value(QStringLiteral("result")).toObject() : QJsonObject{};
        const auto redoneRevision = redoResult.value(QStringLiteral("current"))
                                        .toObject()
                                        .value(QStringLiteral("revision"))
                                        .toInteger(-1);
        const auto documentAfterRedo = getDocument(QStringLiteral("document-after-redo"));
        const auto trackCountAfterRedo = documentAfterRedo
                                             ? documentAfterRedo->value(QStringLiteral("result"))
                                                   .toObject()
                                                   .value(QStringLiteral("snapshot"))
                                                   .toObject()
                                                   .value(QStringLiteral("statistics"))
                                                   .toObject()
                                                   .value(QStringLiteral("track_count"))
                                                   .toInteger(-1)
                                             : -1;
        if (!redo || redo->contains(QStringLiteral("error")) ||
            !redoResult.value(QStringLiteral("changed")).toBool() ||
            redoneRevision != undoneRevision + 1 || !documentAfterRedo ||
            documentAfterRedo->contains(QStringLiteral("error")) ||
            trackCountAfterRedo != initialTrackCount + 1) {
            return fail(QStringLiteral("Native history.redo did not restore the inserted track: %1")
                            .arg(redo ? compactJson(*redo) : exchangeError));
        }

        const auto startupProjectPath =
            isolatedRoot.filePath(QStringLiteral("headless-startup.dspx"));
        const auto startupProjectSave = nativeExchange(
            manager, nativeEndpoint,
            nativeRequest(QStringLiteral("save-startup-project"),
                          QStringLiteral("documents.save_as"),
                          QJsonObject{
                              {QStringLiteral("document_id"),       documentId                   },
                              {QStringLiteral("expected_revision"), redoneRevision               },
                              {QStringLiteral("path"),              startupProjectPath            },
                              {QStringLiteral("overwrite_policy"),  QStringLiteral("overwrite")},
        }),
            exchangeError, 10000);
        if (!startupProjectSave || startupProjectSave->contains(QStringLiteral("error")) ||
            !QFileInfo::exists(startupProjectPath)) {
            return fail(QStringLiteral("Could not prepare the startup-project fixture: %1")
                            .arg(startupProjectSave ? compactJson(*startupProjectSave)
                                                    : exchangeError));
        }
        const auto forwardedStartupProjectPath =
            isolatedRoot.filePath(QStringLiteral("headless-forwarded-startup.dspx"));
        if (!QFile::copy(startupProjectPath, forwardedStartupProjectPath)) {
            return fail(QStringLiteral("Could not prepare the forwarded startup-project fixture"));
        }

        const auto guiOnly = nativeExchange(
            manager, nativeEndpoint,
            nativeRequest(QStringLiteral("gui-only"), QStringLiteral("track_panel.set_viewport"),
                          QJsonObject{
                              {QStringLiteral("window_id"), 42}
        }),
            exchangeError);
        const auto guiOnlyError =
            guiOnly ? guiOnly->value(QStringLiteral("error")).toObject() : QJsonObject{};
        const auto guiOnlyData = guiOnlyError.value(QStringLiteral("data")).toObject();
        if (!guiOnly || guiOnlyError.value(QStringLiteral("code")).toInt() != -32000 ||
            guiOnlyData.value(QStringLiteral("code")) !=
                QStringLiteral("host_capability_unavailable") ||
            guiOnlyData.value(QStringLiteral("operation_id")) !=
                QStringLiteral("track_panel.set_viewport") ||
            guiOnlyData.contains(QStringLiteral("field_path"))) {
            return fail(QStringLiteral("GUI-only Host gate did not precede window schema: %1")
                            .arg(guiOnly ? compactJson(*guiOnly) : exchangeError));
        }

        const auto dirtyInsertion = insertTrack(QStringLiteral("dirty-insert"), redoneRevision,
                                                QStringLiteral("insert-dirty"));
        if (!dirtyInsertion || dirtyInsertion->contains(QStringLiteral("error"))) {
            return fail(QStringLiteral("Could not make the headless document dirty: %1")
                            .arg(dirtyInsertion ? compactJson(*dirtyInsertion) : exchangeError));
        }
        const auto rejectedExit =
            nativeExchange(manager, nativeEndpoint,
                           nativeRequest(QStringLiteral("exit-rejected"),
                                         QStringLiteral("application.request_exit")),
                           exchangeError);
        const auto rejectedExitData = rejectedExit ? rejectedExit->value(QStringLiteral("error"))
                                                         .toObject()
                                                         .value(QStringLiteral("data"))
                                                         .toObject()
                                                   : QJsonObject{};
        if (!rejectedExit ||
            rejectedExitData.value(QStringLiteral("code")) != QStringLiteral("busy") ||
            rejectedExitData.value(QStringLiteral("field_path")) !=
                QStringLiteral("discard_changes") ||
            editor.state() != QProcess::Running) {
            return fail(QStringLiteral("Dirty headless exit was not rejected: %1")
                            .arg(rejectedExit ? compactJson(*rejectedExit) : exchangeError));
        }

        const auto acceptedExit =
            nativeExchange(manager, nativeEndpoint,
                           nativeRequest(QStringLiteral("exit-accepted"),
                                         QStringLiteral("application.request_exit"),
                                         QJsonObject{
                                             {QStringLiteral("discard_changes"), true}
        }),
                           exchangeError, 10000);
        const auto acceptedExitResult =
            acceptedExit ? acceptedExit->value(QStringLiteral("result")).toObject() : QJsonObject{};
        if (!acceptedExit || acceptedExit->contains(QStringLiteral("error")) ||
            !acceptedExitResult.value(QStringLiteral("accepted")).toBool() ||
            acceptedExitResult.value(QStringLiteral("action")) != QStringLiteral("exit") ||
            !acceptedExitResult.value(QStringLiteral("discard_changes")).toBool() ||
            !editor.waitForFinished(15000) || editor.exitStatus() != QProcess::NormalExit ||
            editor.exitCode() != 0) {
            return fail(QStringLiteral("Discarded headless exit did not complete cleanly: %1; %2")
                            .arg(acceptedExit ? compactJson(*acceptedExit) : exchangeError,
                                 processDiagnostics(editor, appDataRoot, controlPort)));
        }
        watcher.stop();
        if (!waitUntil([&] { return !tcpListenerAvailable(controlPort); }, 5000) ||
            !waitUntil([&] { return !localServiceAvailable(serviceName); }, 5000)) {
            return fail(
                QStringLiteral("Graceful headless exit left a listener or Primary service"));
        }

#ifdef Q_OS_WIN
        struct TerminationSignalCase {
            DWORD value;
            QString name;
        };
        const std::array terminationSignals{
            TerminationSignalCase{CTRL_BREAK_EVENT, QStringLiteral("CTRL_BREAK_EVENT")},
        };
        signalEditor.setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments *arguments) {
                arguments->flags |= CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE;
                arguments->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
                arguments->startupInfo->wShowWindow = SW_HIDE;
            });
#else
        struct TerminationSignalCase {
            int value;
            QString name;
        };
        const std::array terminationSignals{
            TerminationSignalCase{SIGINT, QStringLiteral("SIGINT")},
            TerminationSignalCase{SIGTERM, QStringLiteral("SIGTERM")},
        };
#endif
        signalEditor.setProcessEnvironment(environment);
        signalEditor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        signalEditor.setProcessChannelMode(QProcess::SeparateChannels);
        for (const auto &terminationSignal : terminationSignals) {
            QTcpServer signalPortProbe;
            if (!signalPortProbe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0)) {
                return fail(QStringLiteral("Could not allocate the %1 control port: %2")
                                .arg(terminationSignal.name, signalPortProbe.errorString()));
            }
            const auto signalPort = signalPortProbe.serverPort();
            signalPortProbe.close();
            const QUrl signalEndpoint(
                QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(signalPort));

            signalEditor.start(
                editorPath, {QStringLiteral("--headless"), QStringLiteral("--no-mcp"),
                             QStringLiteral("--control-level"), QStringLiteral("l3"),
                             QStringLiteral("--control-port"), QString::number(signalPort)});
            if (!signalEditor.waitForStarted(10000)) {
                return fail(QStringLiteral("%1 headless editor failed to start: %2")
                                .arg(terminationSignal.name, signalEditor.errorString()));
            }

            QString signalExchangeError;
            std::optional<QJsonObject> signalStatusResponse;
            const auto signalReady = waitUntil(
                [&] {
                    if (signalEditor.state() == QProcess::NotRunning)
                        return true;
                    const auto response = nativeExchange(
                        manager, signalEndpoint,
                        nativeRequest(QStringLiteral("signal-status"),
                                      QStringLiteral("application.get_status")),
                        signalExchangeError, 500);
                    if (!response || !response->value(QStringLiteral("result")).isObject())
                        return false;
                    signalStatusResponse = response;
                    return true;
                },
                45000);
            if (!signalReady || !signalStatusResponse) {
                return fail(QStringLiteral("%1 headless endpoint did not become ready: %2; %3")
                                .arg(terminationSignal.name, signalExchangeError,
                                     processDiagnostics(signalEditor, appDataRoot, signalPort)));
            }
            const auto signalDocuments = signalStatusResponse->value(QStringLiteral("result"))
                                             .toObject()
                                             .value(QStringLiteral("documents"))
                                             .toArray();
            if (signalDocuments.size() != 1) {
                return fail(QStringLiteral("%1 status did not expose one document")
                                .arg(terminationSignal.name));
            }
            const auto signalDocument = signalDocuments.first().toObject();
            const auto signalDocumentId =
                signalDocument.value(QStringLiteral("document_id")).toString();
            const auto signalRevision =
                signalDocument.value(QStringLiteral("revision")).toInteger(-1);
            const auto signalDirtyInsertion = nativeExchange(
                manager, signalEndpoint,
                nativeRequest(QStringLiteral("signal-dirty-insert"),
                              QStringLiteral("tracks.insert"),
                              QJsonObject{
                                  {QStringLiteral("document_id"), signalDocumentId},
                                  {QStringLiteral("expected_revision"), signalRevision},
                                  {QStringLiteral("index"), 0},
                                  {QStringLiteral("tracks"),
                                   QJsonArray{QJsonObject{
                                       {QStringLiteral("client_ref"),
                                        QStringLiteral("signal-dirty-track")},
                                       {QStringLiteral("name"), QStringLiteral("Signal Track")},
                                       {QStringLiteral("color_index"), 0},
                                   }}},
                }),
                signalExchangeError);
            if (signalDocumentId.isEmpty() || signalRevision < 0 || !signalDirtyInsertion ||
                signalDirtyInsertion->contains(QStringLiteral("error")) ||
                !signalDirtyInsertion->value(QStringLiteral("result"))
                     .toObject()
                     .value(QStringLiteral("changed"))
                     .toBool()) {
                return fail(QStringLiteral("Could not make the %1 document dirty: %2")
                                .arg(terminationSignal.name,
                                     signalDirtyInsertion ? compactJson(*signalDirtyInsertion)
                                                          : signalExchangeError));
            }

#ifdef Q_OS_WIN
            QString sendError;
            if (!sendConsoleControlEvent(signalEditor.processId(), terminationSignal.value,
                                         sendError)) {
                return fail(QStringLiteral("Could not send %1: %2")
                                .arg(terminationSignal.name, sendError));
            }
#else
            if (::kill(static_cast<pid_t>(signalEditor.processId()), terminationSignal.value) == -1) {
                return fail(QStringLiteral("Could not send %1: errno %2")
                                .arg(terminationSignal.name)
                                .arg(errno));
            }
#endif
            if (!signalEditor.waitForFinished(15000) ||
                signalEditor.exitStatus() != QProcess::NormalExit || signalEditor.exitCode() != 0) {
                return fail(QStringLiteral("%1 did not cause a clean headless exit: %2")
                                .arg(terminationSignal.name,
                                     processDiagnostics(signalEditor, appDataRoot, signalPort)));
            }
            const auto signalOutput = QString::fromUtf8(signalEditor.readAllStandardOutput()) +
                                      QString::fromUtf8(signalEditor.readAllStandardError());
            if (!signalOutput.contains(terminationSignal.name)) {
                return fail(QStringLiteral("%1 graceful-exit log was not observed: %2")
                                .arg(terminationSignal.name, signalOutput));
            }
            if (!waitUntil([&] { return !tcpListenerAvailable(signalPort); }, 5000) ||
                !waitUntil([&] { return !localServiceAvailable(serviceName); }, 5000)) {
                return fail(QStringLiteral("%1 exit left a listener or Primary service")
                                .arg(terminationSignal.name));
            }
        }

        QTcpServer conflictOwner;
        if (!conflictOwner.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0)) {
            return fail(QStringLiteral("Could not reserve the conflicting control port: %1")
                            .arg(conflictOwner.errorString()));
        }
        const auto conflictingPort = conflictOwner.serverPort();
        conflictingEditor.setProcessEnvironment(environment);
        conflictingEditor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        conflictingEditor.setProcessChannelMode(QProcess::SeparateChannels);
        conflictingEditor.start(
            editorPath, {QStringLiteral("--headless"), QStringLiteral("--no-mcp"),
                         QStringLiteral("--control-level"), QStringLiteral("l3"),
                         QStringLiteral("--control-port"), QString::number(conflictingPort)});
        if (!conflictingEditor.waitForStarted(10000) || !conflictingEditor.waitForFinished(20000) ||
            conflictingEditor.exitStatus() != QProcess::NormalExit ||
            conflictingEditor.exitCode() == 0 || !conflictOwner.isListening()) {
            return fail(
                QStringLiteral("Port-conflicted headless editor did not fail cleanly: %1")
                    .arg(processDiagnostics(conflictingEditor, appDataRoot, conflictingPort)));
        }
        conflictOwner.close();
        if (!waitUntil([&] { return !localServiceAvailable(serviceName); }, 5000) ||
            conflictingEditor.state() != QProcess::NotRunning) {
            return fail(
                QStringLiteral("Port-conflicted headless editor left process or Primary state"));
        }
        QTcpServer releasedPortProbe;
        if (!releasedPortProbe.listen(QHostAddress(QStringLiteral("127.0.0.1")), conflictingPort)) {
            return fail(
                QStringLiteral("Conflicted control port remained occupied after cleanup: %1")
                    .arg(releasedPortProbe.errorString()));
        }

        QTcpServer mcpPortProbe;
        if (!mcpPortProbe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0)) {
            return fail(QStringLiteral("Could not allocate the combined Native/MCP port: %1")
                            .arg(mcpPortProbe.errorString()));
        }
        const auto mcpPort = mcpPortProbe.serverPort();
        mcpPortProbe.close();
        const auto mcpEndpoint = QStringLiteral("http://127.0.0.1:%1/mcp").arg(mcpPort);
        const QUrl combinedNativeEndpoint(
            QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(mcpPort));
        DsConnector::BootstrapWatcher mcpWatcher(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                                 QStringLiteral("1"), serviceName);
        mcpWatcher.start();
        const auto mcpWatcherCleanup = qScopeGuard([&mcpWatcher] { mcpWatcher.stop(); });

        restartWorkingDirectory =
            QDir(isolatedRoot.path()).filePath(QStringLiteral("restart-working-directory"));
        if (!QDir().mkpath(restartWorkingDirectory))
            return fail(QStringLiteral("Could not create the isolated restart working directory"));
        restartArguments = {
            QStringLiteral("--headless"),      QStringLiteral("--mcp"),
            QStringLiteral("--control-level"), QStringLiteral("l3"),
            QStringLiteral("--control-port"),  QString::number(mcpPort),
            startupProjectPath,
        };
        mcpEditor.setProcessEnvironment(environment);
        mcpEditor.setWorkingDirectory(restartWorkingDirectory);
        mcpEditor.setProcessChannelMode(QProcess::SeparateChannels);
        mcpEditor.start(editorPath, restartArguments);
        if (!mcpEditor.waitForStarted(10000)) {
            return fail(QStringLiteral("Combined Native/MCP headless editor failed to start: %1")
                            .arg(mcpEditor.errorString()));
        }
        restartSourceProcessId = mcpEditor.processId();
        QString forwardError;
        SingleInstanceCoordinator forwardingClient(editorDataDirectory, serviceName);
        const SingleInstanceRequest forwardedStartupRequest{
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            SingleInstanceCommand::OpenProjects,
            {forwardedStartupProjectPath},
        };
        if (!forwardingClient.forwardRequest(forwardedStartupRequest, forwardError)) {
            return fail(QStringLiteral("Could not forward a project during headless startup: %1; %2")
                            .arg(forwardError,
                                 processDiagnostics(mcpEditor, appDataRoot, mcpPort)));
        }

        const auto mcpBootstrapSettled = waitUntil(
            [&] {
                return (mcpWatcher.observation().snapshot &&
                        mcpWatcher.observation().snapshot->result.state ==
                            SingleInstanceAutomationState::ServerReady) ||
                       mcpEditor.state() == QProcess::NotRunning;
            },
            45000);
        if (!mcpBootstrapSettled || !mcpWatcher.observation().snapshot) {
            return fail(QStringLiteral("Combined Native/MCP bootstrap did not become ready: %1; %2")
                            .arg(mcpWatcher.observation().error,
                                 processDiagnostics(mcpEditor, appDataRoot, mcpPort)));
        }
        const auto &mcpBootstrap = mcpWatcher.observation().snapshot->result;
        if (mcpBootstrap.hostMode != QStringLiteral("headless") ||
            mcpBootstrap.state != SingleInstanceAutomationState::ServerReady ||
            !mcpBootstrap.serverEnabled || mcpBootstrap.serverEndpoint != mcpEndpoint) {
            return fail(QStringLiteral("Combined Native/MCP bootstrap state was incorrect"));
        }

        std::optional<QJsonObject> combinedStatusResponse;
        const auto combinedNativeSettled = waitUntil(
            [&] {
                if (mcpEditor.state() == QProcess::NotRunning)
                    return true;
                const auto response =
                    nativeExchange(manager, combinedNativeEndpoint,
                                   nativeRequest(QStringLiteral("combined-status"),
                                                 QStringLiteral("application.get_status")),
                                   exchangeError, 500);
                if (!response || !response->value(QStringLiteral("result")).isObject())
                    return false;
                combinedStatusResponse = response;
                return true;
            },
            10000);
        const auto combinedStatus =
            combinedStatusResponse
                ? combinedStatusResponse->value(QStringLiteral("result")).toObject()
                : QJsonObject{};
        if (!combinedNativeSettled || !combinedStatusResponse ||
            combinedStatus.value(QStringLiteral("host_mode")) != QStringLiteral("headless") ||
            combinedStatus.value(QStringLiteral("documents")).toArray().size() != 1 ||
            !combinedStatus.value(QStringLiteral("windows")).toArray().isEmpty()) {
            return fail(
                QStringLiteral("Native route was unavailable beside MCP: %1; %2")
                    .arg(exchangeError, processDiagnostics(mcpEditor, appDataRoot, mcpPort)));
        }

        DsConnector::UpstreamMcpClient mcpClient(QStringLiteral("headless-process-integration"),
                                                 QStringLiteral("1"));
        QString endpointError;
        if (!mcpClient.setEndpoint(mcpEndpoint, &endpointError)) {
            return fail(
                QStringLiteral("Could not configure the direct MCP client: %1").arg(endpointError));
        }
        const auto mcpStatusResponse =
            mcpExchange(mcpClient, QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                        QJsonObject{
                            {QStringLiteral("name"),      QStringLiteral("application.get_status")},
                            {QStringLiteral("arguments"), QJsonObject{}                           },
        },
                        exchangeError, 10000);
        const auto mcpStatus =
            mcpStatusResponse
                ? mcpStatusResponse->value(QStringLiteral("structuredContent")).toObject()
                : QJsonObject{};
        if (!mcpStatusResponse || mcpStatusResponse->value(QStringLiteral("isError")).toBool() ||
            mcpStatus != combinedStatus) {
            return fail(
                QStringLiteral("Native and MCP application status were not equivalent: %1")
                    .arg(mcpStatusResponse ? compactJson(*mcpStatusResponse) : exchangeError));
        }
        QSet<QString> toolNames;
        QString cursor;
        for (int pageIndex = 0; pageIndex < 16; ++pageIndex) {
            QJsonObject params;
            if (!cursor.isEmpty())
                params.insert(QStringLiteral("cursor"), cursor);
            const auto page =
                mcpExchange(mcpClient, QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod),
                            std::move(params), exchangeError, 10000);
            if (!page) {
                return fail(
                    QStringLiteral("Headless MCP tools/list failed: %1").arg(exchangeError));
            }
            const auto tools = page->value(QStringLiteral("tools")).toArray();
            for (const auto &toolValue : tools) {
                const auto name = toolValue.toObject().value(QStringLiteral("name")).toString();
                if (name.isEmpty() || toolNames.contains(name)) {
                    return fail(QStringLiteral("Headless MCP tools/list returned an invalid or "
                                               "duplicate tool name"));
                }
                toolNames.insert(name);
            }
            cursor = page->value(QStringLiteral("nextCursor")).toString();
            if (cursor.isEmpty())
                break;
            if (pageIndex == 15)
                return fail(QStringLiteral("Headless MCP tools/list pagination did not terminate"));
        }
        if (toolNames.size() != 151 ||
            !toolNames.contains(QStringLiteral("application.get_status")) ||
            toolNames.contains(QStringLiteral("track_panel.get_state"))) {
            return fail(QStringLiteral("Headless MCP exposed %1 tools instead of the qualified 151")
                            .arg(toolNames.size()));
        }

        const auto combinedDocument =
            combinedStatus.value(QStringLiteral("documents")).toArray().first().toObject();
        const auto combinedDocumentId =
            combinedDocument.value(QStringLiteral("document_id")).toString();
        const auto combinedRevision =
            combinedDocument.value(QStringLiteral("revision")).toInteger(-1);
        const auto startupLoadedDocument = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("startup-loaded-document"),
                          QStringLiteral("documents.get"),
                          QJsonObject{
                              {QStringLiteral("document_id"), combinedDocumentId}
        }),
            exchangeError, 10000);
        const auto startupLoadedSnapshot =
            startupLoadedDocument
                ? startupLoadedDocument->value(QStringLiteral("result"))
                      .toObject()
                      .value(QStringLiteral("snapshot"))
                      .toObject()
                : QJsonObject{};
        if (!startupLoadedDocument || startupLoadedDocument->contains(QStringLiteral("error")) ||
            QFileInfo(startupLoadedSnapshot.value(QStringLiteral("path")).toString())
                    .canonicalFilePath() !=
                QFileInfo(forwardedStartupProjectPath).canonicalFilePath()) {
            return fail(QStringLiteral("Headless readiness preceded a forwarded startup project: %1")
                            .arg(startupLoadedDocument ? compactJson(*startupLoadedDocument)
                                                       : exchangeError));
        }
        const auto taskTrackInsertion = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("task-track"), QStringLiteral("tracks.insert"),
                          QJsonObject{
                              {QStringLiteral("document_id"),       combinedDocumentId},
                              {QStringLiteral("expected_revision"), combinedRevision  },
                              {QStringLiteral("index"),             0                 },
                              {QStringLiteral("tracks"),
                               QJsonArray{QJsonObject{
                                   {QStringLiteral("client_ref"), QStringLiteral("task-track")},
                                   {QStringLiteral("name"), QStringLiteral("Task Track")},
                                   {QStringLiteral("color_index"), 0},
                               }}                                                     },
        }),
            exchangeError);
        const auto taskTrackResult =
            taskTrackInsertion ? taskTrackInsertion->value(QStringLiteral("result")).toObject()
                               : QJsonObject{};
        const auto taskTrackRevision = taskTrackResult.value(QStringLiteral("current"))
                                           .toObject()
                                           .value(QStringLiteral("revision"))
                                           .toInteger(-1);
        qint64 taskTrackId = -1;
        for (const auto &createdValue :
             taskTrackResult.value(QStringLiteral("created_objects")).toArray()) {
            const auto created = createdValue.toObject();
            const auto object = created.value(QStringLiteral("object")).toObject();
            if (created.value(QStringLiteral("client_ref")) == QStringLiteral("task-track") &&
                object.value(QStringLiteral("kind")) == QStringLiteral("track")) {
                taskTrackId = object.value(QStringLiteral("id")).toInteger(-1);
            }
        }
        if (!taskTrackInsertion || taskTrackInsertion->contains(QStringLiteral("error")) ||
            taskTrackId < 0 || taskTrackRevision != combinedRevision + 1) {
            return fail(
                QStringLiteral("Could not create the Headless async-task track: %1")
                    .arg(taskTrackInsertion ? compactJson(*taskTrackInsertion) : exchangeError));
        }

        const auto audioImport = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("audio-import"), QStringLiteral("audio_clips.import"),
                          QJsonObject{
                              {QStringLiteral("document_id"),       combinedDocumentId              },
                              {QStringLiteral("expected_revision"), taskTrackRevision               },
                              {QStringLiteral("track_id"),          taskTrackId                     },
                              {QStringLiteral("start"),             0                               },
                              {QStringLiteral("path"),              audioPath                       },
                              {QStringLiteral("idempotency_key"),   QStringLiteral("headless-audio")},
        }),
            exchangeError, 10000);
        const auto audioTaskId = audioImport ? audioImport->value(QStringLiteral("result"))
                                                   .toObject()
                                                   .value(QStringLiteral("task_id"))
                                                   .toString()
                                             : QString{};
        if (audioTaskId.isEmpty()) {
            return fail(QStringLiteral("Native audio import did not create a task: %1")
                            .arg(audioImport ? compactJson(*audioImport) : exchangeError));
        }

        QJsonObject audioTask;
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto taskResponse =
                mcpExchange(mcpClient, QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
                            QJsonObject{
                                {QStringLiteral("name"),      QStringLiteral("tasks.get")},
                                {QStringLiteral("arguments"),
                                 QJsonObject{
                                     {QStringLiteral("scope"), QStringLiteral("document")},
                                     {QStringLiteral("document_id"), combinedDocumentId},
                                     {QStringLiteral("task_id"), audioTaskId},
                                 }                                                       },
            },
                            exchangeError, 10000);
            if (!taskResponse || taskResponse->value(QStringLiteral("isError")).toBool()) {
                return fail(QStringLiteral("MCP tasks.get could not read the Native task: %1")
                                .arg(taskResponse ? compactJson(*taskResponse) : exchangeError));
            }
            audioTask = taskResponse->value(QStringLiteral("structuredContent")).toObject();
            const auto state = audioTask.value(QStringLiteral("state")).toString();
            if (state == QStringLiteral("succeeded") || state == QStringLiteral("failed") ||
                state == QStringLiteral("canceled")) {
                break;
            }
            QThread::msleep(50);
        }
        if (audioTask.value(QStringLiteral("state")) != QStringLiteral("succeeded")) {
            return fail(QStringLiteral("Headless audio import task did not succeed: %1")
                            .arg(compactJson(audioTask)));
        }

        const auto audioMutation = audioTask.value(QStringLiteral("result")).toObject();
        const auto savedRevision = audioMutation.value(QStringLiteral("current"))
                                       .toObject()
                                       .value(QStringLiteral("revision"))
                                       .toInteger(-1);
        const auto roundTripPath =
            isolatedRoot.filePath(QStringLiteral("headless-round-trip.dspx"));
        const auto savedAs = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("save-round-trip"), QStringLiteral("documents.save_as"),
                          QJsonObject{
                              {QStringLiteral("document_id"),       combinedDocumentId         },
                              {QStringLiteral("expected_revision"), savedRevision              },
                              {QStringLiteral("path"),              roundTripPath              },
                              {QStringLiteral("overwrite_policy"),  QStringLiteral("overwrite")},
        }),
            exchangeError, 10000);
        const auto saveResult =
            savedAs ? savedAs->value(QStringLiteral("result")).toObject() : QJsonObject{};
        if (savedRevision < 0 || !savedAs || savedAs->contains(QStringLiteral("error")) ||
            saveResult.value(QStringLiteral("current"))
                    .toObject()
                    .value(QStringLiteral("document_id")) != combinedDocumentId ||
            saveResult.value(QStringLiteral("current"))
                    .toObject()
                    .value(QStringLiteral("revision"))
                    .toInteger(-1) != savedRevision ||
            !QFileInfo::exists(roundTripPath) || QFileInfo(roundTripPath).size() <= 0) {
            return fail(QStringLiteral("Headless documents.save_as did not publish a DSPX copy: %1")
                            .arg(savedAs ? compactJson(*savedAs) : exchangeError));
        }

        const auto opened = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("open-round-trip"), QStringLiteral("documents.open"),
                          QJsonObject{
                              {QStringLiteral("current_document_id"), combinedDocumentId      },
                              {QStringLiteral("expected_revision"),   savedRevision           },
                              {QStringLiteral("path"),                roundTripPath           },
                              {QStringLiteral("unsaved_policy"),      QStringLiteral("reject")},
        }),
            exchangeError, 10000);
        const auto openTaskId = opened ? opened->value(QStringLiteral("result"))
                                             .toObject()
                                             .value(QStringLiteral("task_id"))
                                             .toString()
                                       : QString{};
        if (openTaskId.isEmpty()) {
            return fail(QStringLiteral("Headless documents.open did not create a task: %1")
                            .arg(opened ? compactJson(*opened) : exchangeError));
        }

        QJsonObject openTask;
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto currentStatus =
                nativeExchange(manager, combinedNativeEndpoint,
                               nativeRequest(QStringLiteral("open-status-%1").arg(attempt),
                                             QStringLiteral("application.get_status")),
                               exchangeError, 5000);
            const auto currentDocuments = currentStatus
                                              ? currentStatus->value(QStringLiteral("result"))
                                                    .toObject()
                                                    .value(QStringLiteral("documents"))
                                                    .toArray()
                                              : QJsonArray{};
            if (currentDocuments.size() != 1) {
                QThread::msleep(50);
                continue;
            }
            const auto currentDocumentId =
                currentDocuments.first().toObject().value(QStringLiteral("document_id")).toString();
            const auto taskResponse = nativeExchange(
                manager, combinedNativeEndpoint,
                nativeRequest(QStringLiteral("open-task-%1").arg(attempt),
                              QStringLiteral("tasks.get"),
                              QJsonObject{
                                  {QStringLiteral("scope"),       QStringLiteral("document")},
                                  {QStringLiteral("document_id"), currentDocumentId         },
                                  {QStringLiteral("task_id"),     openTaskId                },
            }),
                exchangeError, 5000);
            if (taskResponse && !taskResponse->contains(QStringLiteral("error"))) {
                openTask = taskResponse->value(QStringLiteral("result")).toObject();
                const auto state = openTask.value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("succeeded") || state == QStringLiteral("failed") ||
                    state == QStringLiteral("canceled")) {
                    break;
                }
            }
            QThread::msleep(50);
        }
        const auto openMutation = openTask.value(QStringLiteral("result")).toObject();
        const auto openedDocument = openMutation.value(QStringLiteral("current")).toObject();
        const auto openedDocumentId =
            openedDocument.value(QStringLiteral("document_id")).toString();
        const auto reopenedDocument = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("reopened-document"), QStringLiteral("documents.get"),
                          QJsonObject{
                              {QStringLiteral("document_id"), openedDocumentId}
        }),
            exchangeError, 10000);
        const auto reopenedSnapshot = reopenedDocument
                                          ? reopenedDocument->value(QStringLiteral("result"))
                                                .toObject()
                                                .value(QStringLiteral("snapshot"))
                                                .toObject()
                                          : QJsonObject{};
        const auto reopenedStatistics =
            reopenedSnapshot.value(QStringLiteral("statistics")).toObject();
        if (openTask.value(QStringLiteral("state")) != QStringLiteral("succeeded") ||
            openedDocumentId.isEmpty() || openedDocumentId == combinedDocumentId ||
            openedDocument.value(QStringLiteral("revision")).toInteger(-1) != 0 ||
            !reopenedDocument || reopenedDocument->contains(QStringLiteral("error")) ||
            QFileInfo(reopenedSnapshot.value(QStringLiteral("path")).toString())
                    .canonicalFilePath() != QFileInfo(roundTripPath).canonicalFilePath() ||
            !reopenedSnapshot.value(QStringLiteral("saved")).toBool() ||
            reopenedStatistics.value(QStringLiteral("track_count")).toInteger() < 1 ||
            reopenedStatistics.value(QStringLiteral("clip_count")).toInteger() < 1) {
            return fail(QStringLiteral("Headless DSPX round trip did not reopen the saved project: "
                                       "task=%1; document=%2")
                            .arg(compactJson(openTask), reopenedDocument
                                                            ? compactJson(*reopenedDocument)
                                                            : exchangeError));
        }

        const auto originalMcpInstanceId = mcpBootstrap.editorInstanceId;
        const auto originalProcessSnapshot = processSnapshot(restartSourceProcessId);
#ifdef Q_OS_WIN
        if (!originalProcessSnapshot ||
            !processMatches(*originalProcessSnapshot, editorPath, restartWorkingDirectory,
                            restartArguments)) {
            return fail(QStringLiteral("Could not verify the original restart process parameters"));
        }
#endif
        const auto restartResponse = nativeExchange(
            manager, combinedNativeEndpoint,
            nativeRequest(QStringLiteral("restart"), QStringLiteral("application.request_restart"),
                          QJsonObject{
                              {QStringLiteral("discard_changes"), true}
        }),
            exchangeError, 10000);
        const auto restartResult = restartResponse
                                       ? restartResponse->value(QStringLiteral("result")).toObject()
                                       : QJsonObject{};
        if (!restartResponse || restartResponse->contains(QStringLiteral("error")) ||
            !restartResult.value(QStringLiteral("accepted")).toBool() ||
            restartResult.value(QStringLiteral("action")) != QStringLiteral("restart") ||
            !restartResult.value(QStringLiteral("discard_changes")).toBool() ||
            !mcpEditor.waitForFinished(15000) || mcpEditor.exitStatus() != QProcess::NormalExit ||
            mcpEditor.exitCode() != 0) {
            return fail(
                QStringLiteral("Headless restart request did not retire the old process: %1; %2")
                    .arg(restartResponse ? compactJson(*restartResponse) : exchangeError,
                         processDiagnostics(mcpEditor, appDataRoot, mcpPort)));
        }

        const auto restartedReady = waitUntil(
            [&] {
                const auto &observation = mcpWatcher.observation();
                if (!observation.snapshot ||
                    observation.snapshot->result.editorInstanceId == originalMcpInstanceId ||
                    observation.snapshot->result.state !=
                        SingleInstanceAutomationState::ServerReady) {
                    return false;
                }
                restartedProcessId = observation.snapshot->primaryProcessId;
                return restartedProcessId > 0 && restartedProcessId != restartSourceProcessId;
            },
            45000);
        if (!restartedReady) {
            restartedProcessId = findOwnedProcess(editorPath, restartWorkingDirectory,
                                                  restartArguments, restartSourceProcessId);
            return fail(QStringLiteral("Restarted Headless instance did not become ready: %1")
                            .arg(mcpWatcher.observation().error));
        }
        const auto restartedSnapshot = processSnapshot(restartedProcessId);
#ifdef Q_OS_WIN
        if (!restartedSnapshot || !originalProcessSnapshot ||
            !processMatches(*restartedSnapshot, editorPath, restartWorkingDirectory,
                            restartArguments) ||
            restartedSnapshot->arguments != originalProcessSnapshot->arguments ||
            normalizedPath(restartedSnapshot->currentDirectory) !=
                normalizedPath(originalProcessSnapshot->currentDirectory)) {
            return fail(
                QStringLiteral("Restarted process did not preserve executable, args, or cwd"));
        }
#endif

        const auto restartedStatusResponse =
            nativeExchange(manager, combinedNativeEndpoint,
                           nativeRequest(QStringLiteral("restarted-status"),
                                         QStringLiteral("application.get_status")),
                           exchangeError, 10000);
        const auto restartedStatus =
            restartedStatusResponse
                ? restartedStatusResponse->value(QStringLiteral("result")).toObject()
                : QJsonObject{};
        const auto restartedInstanceId =
            restartedStatus.value(QStringLiteral("editor_instance_id")).toString();
        if (!restartedStatusResponse ||
            restartedStatusResponse->contains(QStringLiteral("error")) ||
            restartedInstanceId.isEmpty() || restartedInstanceId == originalMcpInstanceId ||
            restartedInstanceId != mcpWatcher.observation().snapshot->result.editorInstanceId ||
            restartedStatus.value(QStringLiteral("host_mode")) != QStringLiteral("headless")) {
            return fail(
                QStringLiteral("Restarted Headless status did not expose a new instance: %1")
                    .arg(restartedStatusResponse ? compactJson(*restartedStatusResponse)
                                                 : exchangeError));
        }

        const auto restartedExit =
            nativeExchange(manager, combinedNativeEndpoint,
                           nativeRequest(QStringLiteral("restarted-exit"),
                                         QStringLiteral("application.request_exit")),
                           exchangeError, 10000);
        const auto restartedExitResult =
            restartedExit ? restartedExit->value(QStringLiteral("result")).toObject()
                          : QJsonObject{};
        if (!restartedExit || restartedExit->contains(QStringLiteral("error")) ||
            !restartedExitResult.value(QStringLiteral("accepted")).toBool() ||
            !waitUntil([&] { return !processIsRunning(restartedProcessId); }, 15000)) {
            return fail(QStringLiteral("Restarted Headless process did not exit cleanly: %1")
                            .arg(restartedExit ? compactJson(*restartedExit) : exchangeError));
        }
        restartedProcessId = 0;
        mcpWatcher.stop();
        if (!waitUntil([&] { return !tcpListenerAvailable(mcpPort); }, 5000) ||
            !waitUntil([&] { return !localServiceAvailable(serviceName); }, 5000)) {
            return fail(
                QStringLiteral("Restarted Native/MCP host left a listener or Primary service"));
        }

        QTcpServer competitionPortProbe;
        if (!competitionPortProbe.listen(QHostAddress(QStringLiteral("127.0.0.1")), 0)) {
            return fail(QStringLiteral("Could not allocate the Primary competition port: %1")
                            .arg(competitionPortProbe.errorString()));
        }
        const auto competitionPort = competitionPortProbe.serverPort();
        competitionPortProbe.close();
        const QUrl competitionNativeEndpoint(
            QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(competitionPort));
        competitionHeadless.setProcessEnvironment(environment);
        competitionHeadless.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        competitionHeadless.setProcessChannelMode(QProcess::SeparateChannels);
        competitionHeadless.start(
            editorPath, {QStringLiteral("--headless"), QStringLiteral("--no-mcp"),
                         QStringLiteral("--control-level"), QStringLiteral("l3"),
                         QStringLiteral("--control-port"), QString::number(competitionPort)});
        if (!competitionHeadless.waitForStarted(10000)) {
            return fail(QStringLiteral("Headless Primary competition host failed to start: %1")
                            .arg(competitionHeadless.errorString()));
        }
        DsConnector::BootstrapWatcher competitionWatcher(
            QUuid::createUuid().toString(QUuid::WithoutBraces), QStringLiteral("1"), serviceName);
        competitionWatcher.start();
        const auto competitionWatcherCleanup =
            qScopeGuard([&competitionWatcher] { competitionWatcher.stop(); });
        std::optional<QJsonObject> competitionStatusResponse;
        const auto competitionReady = waitUntil(
            [&] {
                if (competitionHeadless.state() == QProcess::NotRunning)
                    return true;
                const auto response =
                    nativeExchange(manager, competitionNativeEndpoint,
                                   nativeRequest(QStringLiteral("competition-status"),
                                                 QStringLiteral("application.get_status")),
                                   exchangeError, 500);
                if (!response || !response->value(QStringLiteral("result")).isObject())
                    return false;
                competitionStatusResponse = response;
                return competitionWatcher.observation().snapshot.has_value();
            },
            45000);
        if (!competitionReady || !competitionStatusResponse ||
            !competitionWatcher.observation().snapshot) {
            return fail(
                QStringLiteral("Headless Primary competition host did not become ready: %1")
                    .arg(processDiagnostics(competitionHeadless, appDataRoot, competitionPort)));
        }
        const auto competitionInstanceId =
            competitionStatusResponse->value(QStringLiteral("result"))
                .toObject()
                .value(QStringLiteral("editor_instance_id"))
                .toString();
        const auto competitionProcessId = competitionHeadless.processId();
        if (competitionWatcher.observation().snapshot->primaryProcessId != competitionProcessId ||
            competitionWatcher.observation().snapshot->result.editorInstanceId !=
                competitionInstanceId) {
            return fail(
                QStringLiteral("Headless competition process was not the published Primary"));
        }

        if (!QDir(platformPluginDirectory).exists()) {
            return fail(QStringLiteral("Qt offscreen platform-plugin directory does not exist"));
        }
        auto guiEnvironment = environment;
        guiEnvironment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        guiEnvironment.insert(QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH"),
                              platformPluginDirectory);
        guiEnvironment.insert(QStringLiteral("QT_OPENGL"), QStringLiteral("software"));
        guiSecondary.setProcessEnvironment(guiEnvironment);
        guiSecondary.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        guiSecondary.setProcessChannelMode(QProcess::SeparateChannels);
        guiSecondary.start(editorPath, {});
        if (!guiSecondary.waitForStarted(10000) || !guiSecondary.waitForFinished(20000) ||
            guiSecondary.exitStatus() != QProcess::NormalExit || guiSecondary.exitCode() != 0 ||
            competitionHeadless.state() != QProcess::Running ||
            !competitionWatcher.observation().snapshot ||
            competitionWatcher.observation().snapshot->primaryProcessId != competitionProcessId ||
            competitionWatcher.observation().snapshot->result.editorInstanceId !=
                competitionInstanceId) {
            return fail(QStringLiteral("GUI secondary did not forward to the Headless Primary: %1")
                            .arg(processDiagnostics(guiSecondary, appDataRoot, competitionPort)));
        }

        const auto competitionExit =
            nativeExchange(manager, competitionNativeEndpoint,
                           nativeRequest(QStringLiteral("competition-exit"),
                                         QStringLiteral("application.request_exit")),
                           exchangeError, 10000);
        if (!competitionExit || competitionExit->contains(QStringLiteral("error")) ||
            !competitionHeadless.waitForFinished(15000) ||
            competitionHeadless.exitStatus() != QProcess::NormalExit ||
            competitionHeadless.exitCode() != 0) {
            return fail(QStringLiteral("Headless competition Primary did not exit cleanly: %1")
                            .arg(competitionExit ? compactJson(*competitionExit) : exchangeError));
        }
        competitionWatcher.stop();
        if (!waitUntil([&] { return !tcpListenerAvailable(competitionPort); }, 5000) ||
            !waitUntil([&] { return !localServiceAvailable(serviceName); }, 5000)) {
            return fail(QStringLiteral("Primary competition left listener or QLocal state"));
        }

        QTextStream(stdout)
            << "Validated QCore-only headless Native workflow, Bootstrap state, no windows, "
               "startup-project readiness, edit/undo/redo, async file tasks, restart, Primary "
               "competition, console termination, and MCP coexistence"
            << Qt::endl;
        return true;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 3) {
        QTextStream(stderr) << "FAILED: Expected the editor path and Qt platform-plugin directory"
                            << Qt::endl;
        return 2;
    }
    return runIntegration(application.arguments().at(1), application.arguments().at(2)) ? 0 : 1;
}
