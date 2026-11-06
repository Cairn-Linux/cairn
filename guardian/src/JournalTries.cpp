// SPDX-License-Identifier: Apache-2.0
#include "JournalTries.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>

#include <limits>
#include <utility>

namespace {

// audit's record types: a password check, and faillock's lock.
const QString userAuth = QStringLiteral("1100");
const QString loginFailures = QStringLiteral("2100");
// syslog's authpriv, where PAM's modules write.
const QString authpriv = QStringLiteral("10");
// auid when no one has logged in, as for the login screen.
constexpr qint64 noLogin = 4294967295;
// The longest login name worth asking NSS about.
constexpr qsizetype maxName = 256;

// The lock screen's program, the only one whose faillock line is read.
bool isLockScreen(const QString& program) {
    return program == QLatin1String("/usr/libexec/kscreenlocker_greet") ||
           program == QLatin1String("/usr/lib/kscreenlocker_greet");
}

// A field as text. journalctl gives a field that is not UTF-8, or that is
// in the entry twice, as an array, which counts as no field.
std::optional<QString> text(const QJsonObject& entry, const QString& field) {
    const QJsonValue value = entry.value(field);
    if (!value.isString()) {
        return std::nullopt;
    }
    return value.toString();
}

// A field of plain digits, as journald writes numbers, and no bigger than
// max.
std::optional<qint64> number(const QJsonObject& entry, const QString& field, qint64 max) {
    const std::optional<QString> value = text(entry, field);
    if (!value || value->isEmpty() || value->size() > 19) {
        return std::nullopt;
    }
    for (const QChar c : *value) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) {
            return std::nullopt;
        }
    }
    bool ok = false;
    const qint64 result = value->toLongLong(&ok);
    if (!ok || result > max) {
        return std::nullopt;
    }
    return result;
}

std::optional<qint64> time(const QJsonObject& entry) {
    return number(entry, QStringLiteral("__REALTIME_TIMESTAMP"),
                  std::numeric_limits<qint64>::max());
}

bool fieldIs(const QJsonObject& entry, const QString& field, const QString& value) {
    return text(entry, field) == value;
}

} // namespace

JournalTries::JournalTries(Lookups lookups) : m_lookups(std::move(lookups)) {}

std::optional<QJsonObject> JournalTries::parse(const QByteArray& line) {
    const QJsonDocument document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

TryPlace JournalTries::placeOf(const QString& program) {
    static const QHash<QString, TryPlace> places{
        {QStringLiteral("/usr/libexec/sddm-helper"), TryPlace::LoginScreen},
        {QStringLiteral("/usr/libexec/plasmalogin-helper"), TryPlace::LoginScreen},
        {QStringLiteral("/usr/bin/unix_chkpwd"), TryPlace::OwnLogin},
        {QStringLiteral("/usr/sbin/unix_chkpwd"), TryPlace::OwnLogin},
        {QStringLiteral("/usr/bin/sudo"), TryPlace::Sudo},
        {QStringLiteral("/usr/bin/su"), TryPlace::Su},
        {QStringLiteral("/usr/lib/polkit-1/polkit-agent-helper-1"), TryPlace::PasswordBox},
        {QStringLiteral("/usr/libexec/polkit-agent-helper-1"), TryPlace::PasswordBox},
        {QStringLiteral("/usr/bin/login"), TryPlace::Console},
    };
    return places.value(program, TryPlace::Other);
}

QString JournalTries::plainName(const QString& program) {
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,63}$"));
    const QString name = program.section(QLatin1Char('/'), -1);
    return plain.match(name).hasMatch() ? name : QString();
}

std::optional<Account> JournalTries::accountNamed(const QJsonObject& entry,
                                                  const QString& field) const {
    const std::optional<QString> name = text(entry, field);
    if (!name || name->isEmpty() || name->size() > maxName) {
        return std::nullopt;
    }
    return m_lookups.byName(*name);
}

std::optional<PasswordTry> JournalTries::tryFrom(const QJsonObject& entry) const {
    if (!fieldIs(entry, QStringLiteral("_TRANSPORT"), QStringLiteral("audit")) ||
        !fieldIs(entry, QStringLiteral("_AUDIT_TYPE"), userAuth) ||
        !fieldIs(entry, QStringLiteral("AUDIT_FIELD_RES"), QStringLiteral("failed"))) {
        return std::nullopt;
    }
    // A password check through PAM, never sshd's key checks or systemd's
    // credentials, which are records of the same type.
    const std::optional<QString> op = text(entry, QStringLiteral("AUDIT_FIELD_OP"));
    if (op != QLatin1String("PAM:authentication") && op != QLatin1String("PAM:unix_chkpwd")) {
        return std::nullopt;
    }
    const std::optional<qint64> when = time(entry);
    if (!when) {
        return std::nullopt;
    }
    PasswordTry found;
    found.time = *when;
    found.account = accountNamed(entry, QStringLiteral("AUDIT_FIELD_ACCT"));
    const QString program = text(entry, QStringLiteral("AUDIT_FIELD_EXE")).value_or(QString());
    found.place = placeOf(program);
    if (found.place == TryPlace::Other) {
        found.program = plainName(program);
    }
    found.pid =
        number(entry, QStringLiteral("_PID"), std::numeric_limits<qint64>::max()).value_or(0);
    // The login the try was made from, which su and sudo keep whoever they
    // run as. The login screen has none.
    const std::optional<qint64> login = number(entry, QStringLiteral("_AUDIT_LOGINUID"), noLogin);
    if (login && *login != noLogin && (!found.account || found.account->uid != *login)) {
        Account gone;
        gone.uid = static_cast<unsigned int>(*login);
        found.from = m_lookups.byUid(gone.uid).value_or(gone);
    }
    return found;
}

std::optional<LockEvent> JournalTries::lockFrom(const QJsonObject& entry) const {
    const std::optional<qint64> when = time(entry);
    if (!when) {
        return std::nullopt;
    }
    LockEvent lock;
    lock.time = *when;
    lock.pid =
        number(entry, QStringLiteral("_PID"), std::numeric_limits<qint64>::max()).value_or(0);
    if (fieldIs(entry, QStringLiteral("_TRANSPORT"), QStringLiteral("audit")) &&
        fieldIs(entry, QStringLiteral("_AUDIT_TYPE"), loginFailures)) {
        // faillock names the locked account by its number.
        const std::optional<qint64> uid =
            number(entry, QStringLiteral("AUDIT_FIELD_SUID"), noLogin);
        const std::optional<Account> account =
            uid ? m_lookups.byUid(static_cast<unsigned int>(*uid)) : std::nullopt;
        if (!account) {
            return std::nullopt;
        }
        const QString program = text(entry, QStringLiteral("AUDIT_FIELD_EXE")).value_or(QString());
        lock.account = *account;
        lock.place = placeOf(program);
        lock.program = lock.place == TryPlace::Other ? plainName(program) : QString();
        return lock;
    }
    // faillock's line, as pam_faillock 1.7.2 writes it, from the lock
    // screen's password stack or its fingerprint stack (ADR-0051).
    static const QRegularExpression line(QStringLiteral(
        "^pam_faillock\\((kde|kde-fingerprint):auth\\): Consecutive login failures for user "
        "([^\\n]+) account temporarily locked$"));
    const std::optional<QString> message = text(entry, QStringLiteral("MESSAGE"));
    const std::optional<qint64> sender = number(entry, QStringLiteral("_UID"), noLogin);
    if (!fieldIs(entry, QStringLiteral("_TRANSPORT"), QStringLiteral("syslog")) ||
        !fieldIs(entry, QStringLiteral("SYSLOG_FACILITY"), authpriv) || !message || !sender ||
        !isLockScreen(text(entry, QStringLiteral("_EXE")).value_or(QString()))) {
        return std::nullopt;
    }
    const QRegularExpressionMatch match = line.match(*message);
    if (!match.hasMatch() || match.capturedLength(2) > maxName) {
        return std::nullopt;
    }
    const std::optional<Account> account = m_lookups.byName(match.captured(2));
    // Anyone can write this line, and the lock screen's program runs as
    // the person, so only root, or the account itself, is believed.
    if (!account || (*sender != 0 && account->uid != *sender)) {
        return std::nullopt;
    }
    lock.account = *account;
    lock.place = TryPlace::OwnLogin;
    return lock;
}
