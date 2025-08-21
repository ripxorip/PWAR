#pragma once
#include <QObject>
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

private:
    void applyRuntimeConfig();
    
    QString m_status;
    pwar_config_t m_config;
    bool m_initialized;
};
