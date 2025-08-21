#include "PwarController.h"
#include <QDebug>
#include <cstring>
#include <QProcess>
#include <QStandardPaths>

PwarController::PwarController(QObject *parent) 
    : QObject(parent), m_status("Ready"), m_initialized(false) {
    
    // Initialize QSettings with organization and application name
    m_settings = new QSettings("PWAR", "PwarController", this);
    
    // Initialize default config
    strcpy(m_config.stream_ip, "192.168.66.3");
    m_config.stream_port = 8321;
    m_config.passthrough_test = 0;
    m_config.oneshot_mode = 0;
    m_config.buffer_size = 64;
    
    // Populate port lists
    updateInputPorts();
    updateOutputPorts();
    
    // Load saved settings
    loadSettings();
}

PwarController::~PwarController() {
    saveSettings();
    if (m_initialized) {
        pwar_cleanup();
    }
}

QString PwarController::status() const {
    return m_status;
}

void PwarController::setStatus(const QString &status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

bool PwarController::isRunning() const {
    return pwar_is_running();
}

QString PwarController::streamIp() const {
    return QString(m_config.stream_ip);
}

void PwarController::setStreamIp(const QString &ip) {
    QByteArray ipBytes = ip.toUtf8();
    if (strcmp(m_config.stream_ip, ipBytes.constData()) != 0) {
        strcpy(m_config.stream_ip, ipBytes.constData());
        emit streamIpChanged();
        if (pwar_is_running()) {
            setStatus("IP changed - stop and start to apply");
        }
    }
}

int PwarController::streamPort() const {
    return m_config.stream_port;
}

void PwarController::setStreamPort(int port) {
    if (m_config.stream_port != port) {
        m_config.stream_port = port;
        emit streamPortChanged();
        if (pwar_is_running()) {
            setStatus("Port changed - stop and start to apply");
        }
    }
}

bool PwarController::passthroughTest() const {
    return m_config.passthrough_test;
}

void PwarController::setPassthroughTest(bool enabled) {
    if (m_config.passthrough_test != enabled) {
        m_config.passthrough_test = enabled;
        emit passthroughTestChanged();
        applyRuntimeConfig();
    }
}

bool PwarController::oneshotMode() const {
    return m_config.oneshot_mode;
}

void PwarController::setOneshotMode(bool enabled) {
    if (m_config.oneshot_mode != enabled) {
        m_config.oneshot_mode = enabled;
        emit oneshotModeChanged();
        applyRuntimeConfig();
    }
}

int PwarController::bufferSize() const {
    return m_config.buffer_size;
}

void PwarController::setBufferSize(int size) {
    if (m_config.buffer_size != size) {
        m_config.buffer_size = size;
        emit bufferSizeChanged();
        if (pwar_is_running()) {
            setStatus("Buffer size changed - stop and start to apply");
        }
    }
}

QStringList PwarController::outputPorts() const {
    return m_outputPorts;
}

QStringList PwarController::inputPorts() const {
    return m_inputPorts;
}

void PwarController::updateOutputPorts() {
    QProcess process;
    // The output is retrieved with "-i" option, not a bug
    process.start("pw-link", QStringList() << "-i");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    m_outputPorts = output.split('\n', Qt::SkipEmptyParts);
    emit outputPortsChanged();
}

void PwarController::updateInputPorts() {
    QProcess process;
    // The input is retrieved with "-o" option, not a bug
    process.start("pw-link", QStringList() << "-o");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    m_inputPorts = output.split('\n', Qt::SkipEmptyParts);
    emit inputPortsChanged();
}

QString PwarController::selectedInputPort() const {
    return m_selectedInputPort;
}

void PwarController::setSelectedInputPort(const QString &port) {
    if (m_selectedInputPort != port) {
        m_selectedInputPort = port;
        emit selectedInputPortChanged();
    }
}

QString PwarController::selectedOutputLeftPort() const {
    return m_selectedOutputLeftPort;
}

void PwarController::setSelectedOutputLeftPort(const QString &port) {
    if (m_selectedOutputLeftPort != port) {
        m_selectedOutputLeftPort = port;
        emit selectedOutputLeftPortChanged();
    }
}

QString PwarController::selectedOutputRightPort() const {
    return m_selectedOutputRightPort;
}

void PwarController::setSelectedOutputRightPort(const QString &port) {
    if (m_selectedOutputRightPort != port) {
        m_selectedOutputRightPort = port;
        emit selectedOutputRightPortChanged();
    }
}

void PwarController::start() {
    if (!m_initialized) {
        if (pwar_init(&m_config) == 0) {
            m_initialized = true;
            setStatus("Initialized");
        } else {
            setStatus("Failed to initialize");
            return;
        }
    } else {
        // If already initialized, we might need to restart with new config
        // that requires restart (like buffer size, IP, port changes)
        pwar_cleanup();
        m_initialized = false;
        
        if (pwar_init(&m_config) == 0) {
            m_initialized = true;
            setStatus("Reinitialized with current settings");
        } else {
            setStatus("Failed to reinitialize");
            return;
        }
    }
    
    if (pwar_start() == 0) {
        setStatus("Running");
        emit isRunningChanged();
        
        // Link audio ports after a short delay
        linkAudioPorts();
    } else {
        setStatus("Failed to start");
    }
}

void PwarController::stop() {
    if (pwar_stop() == 0) {
        unlinkAudioPorts();
        setStatus("Stopped");
        emit isRunningChanged();
    } else {
        setStatus("Failed to stop");
    }
}

void PwarController::applyRuntimeConfig() {
    if (!m_initialized) {
        return;
    }
    
    int result = pwar_update_config(&m_config);
    if (result == 0) {
        setStatus("Configuration updated");
    } else {
        setStatus("Failed to update configuration");
    }
}

void PwarController::loadSettings() {
    // Load network settings
    QString savedIp = m_settings->value("network/streamIp", QString(m_config.stream_ip)).toString();
    setStreamIp(savedIp);
    
    int savedPort = m_settings->value("network/streamPort", m_config.stream_port).toInt();
    setStreamPort(savedPort);
    
    // Load audio settings
    bool savedPassthrough = m_settings->value("audio/passthroughTest", m_config.passthrough_test).toBool();
    setPassthroughTest(savedPassthrough);
    
    bool savedOneshot = m_settings->value("audio/oneshotMode", m_config.oneshot_mode).toBool();
    setOneshotMode(savedOneshot);
    
    int savedBufferSize = m_settings->value("audio/bufferSize", m_config.buffer_size).toInt();
    setBufferSize(savedBufferSize);
    
    // Load port selections
    m_selectedInputPort = m_settings->value("audio/selectedInputPort", "").toString();
    m_selectedOutputLeftPort = m_settings->value("audio/selectedOutputLeftPort", "").toString();
    m_selectedOutputRightPort = m_settings->value("audio/selectedOutputRightPort", "").toString();
    
    // Emit signals for port selections
    emit selectedInputPortChanged();
    emit selectedOutputLeftPortChanged();
    emit selectedOutputRightPortChanged();
}

void PwarController::saveSettings() {
    if (!m_settings) return;
    
    // Save network settings
    m_settings->setValue("network/streamIp", streamIp());
    m_settings->setValue("network/streamPort", streamPort());
    
    // Save audio settings
    m_settings->setValue("audio/passthroughTest", passthroughTest());
    m_settings->setValue("audio/oneshotMode", oneshotMode());
    m_settings->setValue("audio/bufferSize", bufferSize());
    
    // Save port selections
    m_settings->setValue("audio/selectedInputPort", m_selectedInputPort);
    m_settings->setValue("audio/selectedOutputLeftPort", m_selectedOutputLeftPort);
    m_settings->setValue("audio/selectedOutputRightPort", m_selectedOutputRightPort);
    
    // Force write to disk
    m_settings->sync();
}

void PwarController::linkAudioPorts() {
    if (!m_initialized || !pwar_is_running()) {
        qDebug() << "Cannot link audio ports: PWAR not running";
        return;
    }
    
    // Small delay to ensure PWAR ports are available
    QProcess::execute("sleep", QStringList() << "0.1");
    
    // Link input port
    if (!m_selectedInputPort.isEmpty()) {
        QStringList args;
        args << m_selectedInputPort << "pwar:input";
        
        QProcess process;
        process.start("pw-link", args);
        process.waitForFinished(2000);
        
        if (process.exitCode() == 0) {
            qDebug() << "Successfully linked input:" << m_selectedInputPort << "-> pwar:input";
        } else {
            qDebug() << "Failed to link input port:" << process.readAllStandardError();
        }
    }
    
    // Link left output port
    if (!m_selectedOutputLeftPort.isEmpty()) {
        QStringList args;
        args << "pwar:output-left" << m_selectedOutputLeftPort;
        
        QProcess process;
        process.start("pw-link", args);
        process.waitForFinished(2000);
        
        if (process.exitCode() == 0) {
            qDebug() << "Successfully linked left output: pwar:output-left ->" << m_selectedOutputLeftPort;
        } else {
            qDebug() << "Failed to link left output port:" << process.readAllStandardError();
        }
    }
    
    // Link right output port
    if (!m_selectedOutputRightPort.isEmpty()) {
        QStringList args;
        args << "pwar:output-right" << m_selectedOutputRightPort;
        
        QProcess process;
        process.start("pw-link", args);
        process.waitForFinished(2000);
        
        if (process.exitCode() == 0) {
            qDebug() << "Successfully linked right output: pwar:output-right ->" << m_selectedOutputRightPort;
        } else {
            qDebug() << "Failed to link right output port:" << process.readAllStandardError();
        }
    }
    
    setStatus("Running");
}

void PwarController::unlinkAudioPorts() {
    // Unlink all PWAR ports when stopping
    QStringList pwarPorts = {"pwar:input", "pwar:output-left", "pwar:output-right"};
    
    for (const QString &port : pwarPorts) {
        QProcess process;
        process.start("pw-link", QStringList() << "-d" << port);
        process.waitForFinished(1000);
        
        if (process.exitCode() == 0) {
            qDebug() << "Successfully unlinked:" << port;
        }
    }
}
