// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

// Stands in for pam_faillock in the tests: a folder of records of wrong
// tries, written as faillock writes them, and a settings file naming it.
// The records are made here rather than checked in, so every test file
// keeps its licence line.
class StandInFaillock : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl settingsFile READ settingsFile CONSTANT)

public:
    // faillock.h's status bits.
    static constexpr quint16 valid = 0x1;
    static constexpr quint16 fromAnotherComputer = 0x2;
    static constexpr quint16 fromATerminal = 0x4;

    // settings is the settings file's text after its "dir =" line.
    explicit StandInFaillock(const QByteArray& settings = "deny = 3\nunlock_time = 600\n",
                             QObject* parent = nullptr);

    QString dir() const;
    QUrl settingsFile() const;

    // One 64-byte record, as write_tally fills it.
    static QByteArray record(qint64 time, quint16 status = valid,
                             const QByteArray& source = "sddm");

    // Adds a wrong try at time to the account's record.
    Q_INVOKABLE void addFailure(const QString& loginName, double time) const;
    // Writes the account's record as it is given.
    void write(const QString& loginName, const QByteArray& bytes) const;
    // Empties the folder, as if no one had made a wrong try.
    Q_INVOKABLE void reset() const;
    // Writes the settings file again, with settings after its "dir =" line.
    Q_INVOKABLE void setSettings(const QString& settings) const;

private:
    QTemporaryDir m_root;
};
