#pragma once
#include <QObject>

class PwarController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged)
public:
    explicit PwarController(QObject *parent = nullptr);

    QString status() const;
    void setStatus(const QString &status);

    Q_INVOKABLE void doSomething();

signals:
    void statusChanged();

private:
    QString m_status;
};
