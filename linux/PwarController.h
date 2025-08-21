#pragma once
#include <QObject>
#include <QSettings>
#include "libpwar.h"

class PwarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(QString streamIp READ streamIp WRITE setStreamIp NOTIFY streamIpChanged)
    Q_PROPERTY(int streamPort READ streamPort WRITE setStreamPort NOTIFY streamPortChanged)
    Q_PROPERTY(bool passthroughTest READ passthroughTest WRITE setPassthroughTest NOTIFY passthroughTestChanged)
    Q_PROPERTY(bool oneshotMode READ oneshotMode WRITE setOneshotMode NOTIFY oneshotModeChanged)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(QStringList outputPorts READ outputPorts NOTIFY outputPortsChanged)
    Q_PROPERTY(QStringList inputPorts READ inputPorts NOTIFY inputPortsChanged)
    Q_PROPERTY(QString selectedInputPort READ selectedInputPort WRITE setSelectedInputPort NOTIFY selectedInputPortChanged)
    Q_PROPERTY(QString selectedOutputLeftPort READ selectedOutputLeftPort WRITE setSelectedOutputLeftPort NOTIFY selectedOutputLeftPortChanged)
    Q_PROPERTY(QString selectedOutputRightPort READ selectedOutputRightPort WRITE setSelectedOutputRightPort NOTIFY selectedOutputRightPortChanged)

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
    bool oneshotMode() const;
    void setOneshotMode(bool enabled);
    int bufferSize() const;
    void setBufferSize(int size);

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

signals:
    void statusChanged();
    void isRunningChanged();
    void streamIpChanged();
    void streamPortChanged();
    void passthroughTestChanged();
    void oneshotModeChanged();
    void bufferSizeChanged();
    void outputPortsChanged();
    void inputPortsChanged();
    void selectedInputPortChanged();
    void selectedOutputLeftPortChanged();
    void selectedOutputRightPortChanged();

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
};
