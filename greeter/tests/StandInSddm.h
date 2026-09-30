// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>

// Stands in for the `sddm` object SDDM gives a theme: records each login the
// theme asks for, and lets a test answer it with a failure.
class StandInSddm : public QObject {
    Q_OBJECT
    Q_PROPERTY(int logins READ logins NOTIFY loggedIn)
    Q_PROPERTY(QString lastUser READ lastUser NOTIFY loggedIn)
    Q_PROPERTY(QString lastPassword READ lastPassword NOTIFY loggedIn)

public:
    explicit StandInSddm(QObject* parent = nullptr);

    Q_INVOKABLE void login(const QString& user, const QString& password, int sessionIndex);
    Q_INVOKABLE void fail();

    int logins() const;
    QString lastUser() const;
    QString lastPassword() const;

signals:
    void loggedIn();
    void loginFailed();

private:
    int m_logins = 0;
    QString m_lastUser;
    QString m_lastPassword;
};

// Stands in for SDDM's session model: the only thing the theme reads.
class StandInSessions : public QObject {
    Q_OBJECT
    Q_PROPERTY(int lastIndex READ lastIndex CONSTANT)

public:
    explicit StandInSessions(QObject* parent = nullptr);
    int lastIndex() const;
};
