#include "PwarController.h"
#include <QDebug>

PwarController::PwarController(QObject *parent) : QObject(parent), m_status("Ready") {}

QString PwarController::status() const {
    return m_status;
}

void PwarController::setStatus(const QString &status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void PwarController::doSomething() {
    qDebug() << "Click!";
    setStatus("Action performed!");
}
