// SPDX-License-Identifier: Apache-2.0
#include "JournalTries.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStringList>

#include <limits>
#include <utility>

namespace {

// audit's record types: a password check, the account check after it,
// and faillock's lock.
const QString userAuth = QStringLiteral("1100");
const QString userAcct = QStringLiteral("1101");
const QString loginFailures = QStringLiteral("2100");
// syslog's authpriv, where PAM's modules write.
const QString authpriv = QStringLiteral("10");
// auid when no one has logged in, as for the login screen.
constexpr qint64 noLogin = 4294967295;
// The longest login name worth asking NSS about.
constexpr qsizetype maxName = 256;

// The lock screen's programs, the only ones whose faillock line is read,
// by full path in folders only root can write, so a program of a child's
// own with the same name is not one of them. Plasma 6.8 checks passwords
// in kscreenlocker_worker (cairn-min, 2026-11-05); earlier Plasma in
// kscreenlocker_greet.
bool isLockScreen(const QString& program) {
    static const QStringList programs{
        QStringLiteral("/usr/libexec/kscreenlocker_worker"),
        QStringLiteral("/usr/lib/kscreenlocker_worker"),
        QStringLiteral("/usr/libexec/kscreenlocker_greet"),
        QStringLiteral("/usr/lib/kscreenlocker_greet"),
    };
    return programs.contains(program);
}

// A password check through PAM, never sshd's key checks or systemd's
// credentials, which are records of the same type.
bool isPasswordCheck(const std::optional<QString>& op) {
    return op == QLatin1String("PAM:authentication") || op == QLatin1String("PAM:unix_chkpwd");
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

// A level 1 or 2 child, whose password is locked (ADR-0023). The login
// screen lets them in with no password, before the password stack, so
// faillock's part never runs and their login empties nothing; and their
// lock screen has no password to count wrong, so it never locks them.
// The same rule as session/sddm/pam.d/sddm.
bool logsInWithoutPassword(const Account& account) {
    const QStringList& groups = account.groups;
    return (groups.contains(QStringLiteral("cairn-l1")) ||
            groups.contains(QStringLiteral("cairn-l2"))) &&
           !groups.contains(QStringLiteral("cairn-guardian")) &&
           !groups.contains(QStringLiteral("wheel"));
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
    if (!isPasswordCheck(text(entry, QStringLiteral("AUDIT_FIELD_OP")))) {
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
    // Such a child can run unix_chkpwd, the lock screen's check, by hand
    // as often as they like, but no lock can come of it.
    if (found.place == TryPlace::OwnLogin && found.account &&
        logsInWithoutPassword(*found.account)) {
        return std::nullopt;
    }
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
    // The program's path is no proof: in a user namespace a child can run
    // their own program at the lock screen's path (cairn-min, 2026-11-06).
    // A child whose password is locked can never be locked at the lock
    // screen, so a line about one is forged.
    if (logsInWithoutPassword(*account)) {
        return std::nullopt;
    }
    lock.account = *account;
    lock.place = TryPlace::OwnLogin;
    return lock;
}

std::optional<GoodPassword> JournalTries::goodFrom(const QJsonObject& entry) const {
    if (!fieldIs(entry, QStringLiteral("_TRANSPORT"), QStringLiteral("audit")) ||
        !fieldIs(entry, QStringLiteral("AUDIT_FIELD_RES"), QStringLiteral("success"))) {
        return std::nullopt;
    }
    GoodPassword good;
    const std::optional<QString> op = text(entry, QStringLiteral("AUDIT_FIELD_OP"));
    // unix_chkpwd's own record says only whether the password matched.
    // The lock screen's stack goes on to it after faillock has refused
    // (its preauth is "required", not "requisite"), so the right password
    // typed while locked is a unix_chkpwd success and still refused
    // (cairn-min, 2026-11-05, 22:57:51). Only a whole PAM check counts.
    if (fieldIs(entry, QStringLiteral("_AUDIT_TYPE"), userAuth) &&
        op == QLatin1String("PAM:authentication")) {
        good.accountPart = false;
    } else if (fieldIs(entry, QStringLiteral("_AUDIT_TYPE"), userAcct) &&
               op == QLatin1String("PAM:accounting")) {
        good.accountPart = true;
    } else {
        return std::nullopt;
    }
    const std::optional<qint64> when = time(entry);
    const std::optional<qint64> pid =
        number(entry, QStringLiteral("_PID"), std::numeric_limits<qint64>::max());
    const std::optional<Account> account = accountNamed(entry, QStringLiteral("AUDIT_FIELD_ACCT"));
    if (!when || !pid || *pid == 0 || !account) {
        return std::nullopt;
    }
    const QString program = text(entry, QStringLiteral("AUDIT_FIELD_EXE")).value_or(QString());
    if (placeOf(program) == TryPlace::LoginScreen && logsInWithoutPassword(*account)) {
        return std::nullopt;
    }
    good.time = *when;
    good.account = *account;
    good.pid = *pid;
    return good;
}
