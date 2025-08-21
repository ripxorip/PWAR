#include "PwarController.h"
#include <QDebug>
#include <cstring>

PwarController::PwarController(QObject *parent) 
    : QObject(parent), m_status("Ready"), m_initialized(false) {
    
    // Initialize default config
    strcpy(m_config.stream_ip, "192.168.66.3");
    m_config.stream_port = 8321;
    m_config.passthrough_test = 0;
    m_config.oneshot_mode = 0;
    m_config.buffer_size = 64;
}

PwarController::~PwarController() {
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
    } else {
        setStatus("Failed to start");
    }
}

void PwarController::stop() {
    if (pwar_stop() == 0) {
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
