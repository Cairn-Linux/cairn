// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <cstdint>

// Why a login the screen asked for failed, so the screen can say so plainly.
//
// PAM never says why it refused. The one reason the screen can know is that a
// grown-up has the child's Steam open from their own login (ADR-0032): PAM
// refuses a child named in the list of children on a visit, and the screen
// reads the same list. PAM stays the gate; this only picks the words.
class FailedLogin : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The list the root helper writes and PAM reads. Tests give another.
    Q_PROPERTY(QUrl visitsFile READ visitsFile WRITE setVisitsFile NOTIFY visitsFileChanged)

public:
    enum class Reason : std::uint8_t {
        SteamVisit,    // a grown-up has this child's Steam open
        WrongPassword, // a password was given and not taken
        NeedsGrownUp,  // anything else
    };
    Q_ENUM(Reason)

    explicit FailedLogin(QObject* parent = nullptr);

    QUrl visitsFile() const;
    void setVisitsFile(const QUrl& file);

    Q_INVOKABLE Reason reasonFor(const QString& loginName, bool gavePassword) const;

    // Whether the list names the account, one name to a line, as
    // pam_listfile reads it. A list that cannot be read names no one.
    static bool lists(const QString& file, const QString& loginName);

signals:
    void visitsFileChanged();

private:
    QUrl m_visitsFile = QUrl::fromLocalFile(QStringLiteral("/run/cairn/steam-visits"));
};
