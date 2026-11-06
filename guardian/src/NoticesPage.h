// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "TryReader.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QTimeZone>

#include <cstdint>
#include <functional>
#include <optional>

// What the Guardian tool's Notices page shows (ADR-0054): each time wrong
// passwords were typed for an account on this computer in the last 30
// days, where, from whose login, and whether that locked the account, as
// the journal still has them. Family keeps no copy of its own, and sends
// nothing anywhere. Children never see the page: only a Guardian can run
// Family (ADR-0040).
class NoticesPage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The window is given one by main().")
    Q_PROPERTY(State state READ state NOTIFY changed)
    // One sentence or a few for each notice, newest first.
    Q_PROPERTY(QStringList notices READ notices NOTIFY changed)
    // "This computer's record goes back only to …", when it does; empty
    // otherwise. The same for the notices not shown.
    Q_PROPERTY(QString recordStart READ recordStart NOTIFY changed)
    Q_PROPERTY(QString notShown READ notShown NOTIFY changed)
    // How faillock locks and what to do, under the list.
    Q_PROPERTY(QString footer READ footer NOTIFY changed)
    Q_PROPERTY(int days READ days CONSTANT)

public:
    enum class State : std::uint8_t { Reading, Ready, Unreadable, Failed };
    Q_ENUM(State)

    struct Setup {
        TryReader::Setup reader;
        // The Guardian reading, so their own account is "your"; main()
        // gives the real one.
        unsigned int uid = 0;
        QString faillockConf = QStringLiteral("/etc/security/faillock.conf");
        QTimeZone zone = QTimeZone::systemTimeZone();
        // Now, in microseconds since 1970.
        std::function<qint64()> now;
    };

    explicit NoticesPage(Setup setup, QObject* parent = nullptr);

    State state() const;
    QStringList notices() const;
    QString recordStart() const;
    QString notShown() const;
    QString footer() const;
    int days() const;

    // Reads the journal again. Nothing while a read runs.
    Q_INVOKABLE void reload();

signals:
    void changed();

private:
    void read();

    Setup m_setup;
    TryReader* m_reader;
    std::optional<FaillockSettings> m_settings;
    State m_state = State::Reading;
    QStringList m_notices;
    QString m_recordStart;
    QString m_notShown;
    QString m_footer;
};
