#pragma once
#include <QObject>
#include <QSettings>
#include <QTimer>
#include "libpwar.h"

class PwarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(QString streamIp READ streamIp WRITE setStreamIp NOTIFY streamIpChanged)
    Q_PROPERTY(int streamPort READ streamPort WRITE setStreamPort NOTIFY streamPortChanged)
    Q_PROPERTY(bool passthroughTest READ passthroughTest WRITE setPassthroughTest NOTIFY passthroughTestChanged)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(int ringBufferDepth READ ringBufferDepth WRITE setRingBufferDepth NOTIFY ringBufferDepthChanged)
    Q_PROPERTY(QStringList outputPorts READ outputPorts NOTIFY outputPortsChanged)
    Q_PROPERTY(QStringList inputPorts READ inputPorts NOTIFY inputPortsChanged)
    Q_PROPERTY(QString selectedInputPort READ selectedInputPort WRITE setSelectedInputPort NOTIFY selectedInputPortChanged)
    Q_PROPERTY(QString selectedOutputLeftPort READ selectedOutputLeftPort WRITE setSelectedOutputLeftPort NOTIFY selectedOutputLeftPortChanged)
    Q_PROPERTY(QString selectedOutputRightPort READ selectedOutputRightPort WRITE setSelectedOutputRightPort NOTIFY selectedOutputRightPortChanged)
    
    // Latency metrics properties - all 9 values
    Q_PROPERTY(double audioProcMinMs READ audioProcMinMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double audioProcMaxMs READ audioProcMaxMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double audioProcAvgMs READ audioProcAvgMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double jitterMinMs READ jitterMinMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double jitterMaxMs READ jitterMaxMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double jitterAvgMs READ jitterAvgMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double rttMinMs READ rttMinMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double rttMaxMs READ rttMaxMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double rttAvgMs READ rttAvgMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(double ringBufferAvgMs READ ringBufferAvgMs NOTIFY latencyMetricsChanged)
    Q_PROPERTY(uint32_t xruns READ xruns NOTIFY latencyMetricsChanged)
    
    // Current Windows buffer size property
    Q_PROPERTY(int currentWindowsBufferSize READ currentWindowsBufferSize NOTIFY currentWindowsBufferSizeChanged)

public:
    explicit PwarController(QObject *parent = nullptr);
    ~PwarController();

    QString status() const;
    void setStatus(const QString &status);
    
    bool isRunning() const;
    QString streamIp() const;
    void setStreamIp(const QString &ip);
    int streamPort() const;
    void setStreamPort(int port);
    bool passthroughTest() const;
    void setPassthroughTest(bool enabled);
    int bufferSize() const;
    void setBufferSize(int size);
    int ringBufferDepth() const;
    void setRingBufferDepth(int depth);

    QStringList outputPorts() const;
    QStringList inputPorts() const;
    QString selectedInputPort() const;
    void setSelectedInputPort(const QString &port);
    QString selectedOutputLeftPort() const;
    void setSelectedOutputLeftPort(const QString &port);
    QString selectedOutputRightPort() const;
    void setSelectedOutputRightPort(const QString &port);
    
    Q_INVOKABLE void updateOutputPorts();
    Q_INVOKABLE void updateInputPorts();
    Q_INVOKABLE void linkAudioPorts();
    Q_INVOKABLE void unlinkAudioPorts();

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    
    // Latency metrics getters
    double audioProcMinMs() const;
    double audioProcMaxMs() const;
    double audioProcAvgMs() const;
    double jitterMinMs() const;
    double jitterMaxMs() const;
    double jitterAvgMs() const;
    double rttMinMs() const;
    double rttMaxMs() const;
    double rttAvgMs() const;
    double ringBufferAvgMs() const;
    uint32_t xruns() const;
    
    // Current Windows buffer size getter
    int currentWindowsBufferSize() const;
    
    Q_INVOKABLE void updateLatencyMetrics();

signals:
    void statusChanged();
    void isRunningChanged();
    void streamIpChanged();
    void streamPortChanged();
    void passthroughTestChanged();
    void bufferSizeChanged();
    void ringBufferDepthChanged();
    void outputPortsChanged();
    void inputPortsChanged();
    void selectedInputPortChanged();
    void selectedOutputLeftPortChanged();
    void selectedOutputRightPortChanged();
    void latencyMetricsChanged();
    void currentWindowsBufferSizeChanged();

private:
    void applyRuntimeConfig();
    void loadSettings();
    void saveSettings();
    
    QString m_status;
    pwar_config_t m_config;
    bool m_initialized;
    QStringList m_outputPorts;
    QStringList m_inputPorts;
    QString m_selectedInputPort;
    QString m_selectedOutputLeftPort;
    QString m_selectedOutputRightPort;
    QSettings *m_settings;
    
    // Latency metrics
    double m_audioProcMinMs;
    double m_audioProcMaxMs;
    double m_audioProcAvgMs;
    double m_jitterMinMs;
    double m_jitterMaxMs;
    double m_jitterAvgMs;
    double m_rttMinMs;
    double m_rttMaxMs;
    double m_rttAvgMs;
    double m_ringBufferAvgMs;
    uint32_t m_xruns;
    QTimer *m_latencyUpdateTimer;
    
    // Current Windows buffer size
    int m_currentWindowsBufferSize;
};
