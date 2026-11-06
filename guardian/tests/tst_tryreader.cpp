// SPDX-License-Identifier: Apache-2.0
#include "TryReader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

const QString machineId = QStringLiteral("0123456789abcdef0123456789abcdef");

const Account guardian{.uid = 1000,
                       .groups = {"guardian", "wheel", "cairn-guardian"},
                       .login = "guardian",
                       .fullName = "Guardian"};

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray line(const QJsonObject& entry) {
    return QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray failedSudo(qint64 time, int pid) {
    return line({{"__REALTIME_TIMESTAMP", QString::number(time)},
                 {"_TRANSPORT", "audit"},
                 {"_AUDIT_TYPE", "1100"},
                 {"_PID", QString::number(pid)},
                 {"_AUDIT_LOGINUID", "1000"},
                 {"AUDIT_FIELD_OP", "PAM:authentication"},
                 {"AUDIT_FIELD_ACCT", "guardian"},
                 {"AUDIT_FIELD_EXE", "/usr/bin/sudo"},
                 {"AUDIT_FIELD_RES", "failed"}});
}

// A computer in a folder: a machine id, a journal file, and a stand-in
// journalctl that prints each step's lines from a file and writes down how
// it was asked.
class Computer {
public:
    Computer() {
        const QDir dir(m_dir.path());
        dir.mkpath("journal/" + machineId);
        writeFile(dir.filePath("machine-id"), machineId.toLatin1() + '\n');
        writeFile(dir.filePath("journal/" + machineId + "/system.journal"), "LPKSHHRH");
        setStep("oldest", line({{"__REALTIME_TIMESTAMP", "1788499553929683"}}));
        setStep("locks", "");
        setStep("audit", "");
        setExitCode(0);
        writeFile(dir.filePath("journalctl"), "#!/bin/bash\n"
                                              "dir=" +
                                                  m_dir.path().toUtf8() +
                                                  "\n"
                                                  "printf '%s\\n' \"$*\" >> \"$dir/asked\"\n"
                                                  "case \"$*\" in\n"
                                                  "  *--lines=+1*) cat \"$dir/oldest\" ;;\n"
                                                  "  *SYSLOG_FACILITY=10*) cat \"$dir/locks\" ;;\n"
                                                  "  *_AUDIT_TYPE=1100*) cat \"$dir/audit\" ;;\n"
                                                  "esac\n"
                                                  "exit \"$(cat \"$dir/exit-code\")\"\n");
        QFile::setPermissions(dir.filePath("journalctl"),
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }

    void setStep(const QString& name, const QByteArray& lines) const {
        writeFile(QDir(m_dir.path()).filePath(name), lines);
    }

    void setExitCode(int code) const {
        writeFile(QDir(m_dir.path()).filePath("exit-code"), QByteArray::number(code));
    }

    QString path(const QString& name) const {
        return QDir(m_dir.path()).filePath(name);
    }

    QStringList asked() const {
        QFile file(path("asked"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    }

    TryReader::Setup setup() const {
        TryReader::Setup setup;
        setup.journalctl = path("journalctl");
        setup.journalFolders = {path("run-journal"), path("journal")};
        setup.machineId = path("machine-id");
        setup.lookups.byName = [](const QString& name) -> std::optional<Account> {
            return name == guardian.login ? std::optional<Account>(guardian) : std::nullopt;
        };
        setup.lookups.byUid = [](unsigned int uid) -> std::optional<Account> {
            return uid == guardian.uid ? std::optional<Account>(guardian) : std::nullopt;
        };
        return setup;
    }

private:
    QTemporaryDir m_dir;
};

// Reads once and waits for the end.
TryReader::State readAll(TryReader& reader) {
    QSignalSpy finished(&reader, &TryReader::finished);
    reader.read(FaillockSettings{});
    if (!finished.wait(60000)) {
        return TryReader::State::Reading;
    }
    return reader.state();
}

class TryReaderTest : public QObject {
    Q_OBJECT

private slots:
    void readsTheThreeSteps() {
        const Computer computer;
        const qint64 start = 1793808120LL * 1000000;
        computer.setStep("audit", failedSudo(start, 10) + failedSudo(start + 1000000, 11) +
                                      "not json\n" + line({{"MESSAGE", "something else"}}));
        TryReader reader(computer.setup());
        QCOMPARE(readAll(reader), TryReader::State::Ready);
        QCOMPARE(reader.oldest(), 1788499553929683);
        QCOMPARE(reader.notices().shown.size(), 1U);
        QCOMPARE(reader.notices().shown[0].wrong, 2);
        QCOMPARE(reader.notices().shown[0].place, TryPlace::Sudo);
        const QStringList asked = computer.asked();
        QCOMPARE(asked.size(), 3);
        QVERIFY(asked[0].contains("--system --lines=+1"));
        QVERIFY(asked[1].contains("--since=-30d"));
        QVERIFY(asked[1].contains("SYSLOG_FACILITY=10 _EXE=/usr/libexec/kscreenlocker_greet"));
        // Every user's files for the lock screen, the system's for audit.
        QVERIFY(!asked[1].contains("--system"));
        QVERIFY(asked[2].contains("--system"));
        QVERIFY(asked[2].contains("_TRANSPORT=audit _AUDIT_TYPE=1100 AUDIT_FIELD_RES=failed + "
                                  "_TRANSPORT=audit _AUDIT_TYPE=2100"));
    }

    void nothingFound() {
        const Computer computer;
        TryReader reader(computer.setup());
        QCOMPARE(readAll(reader), TryReader::State::Ready);
        QVERIFY(reader.notices().shown.empty());
    }

    // journalctl run without the wheel group succeeds with only the
    // account's own entries, so the reader checks the file first.
    void aJournalThisAccountCannotRead() {
        if (geteuid() == 0) {
            QSKIP("root reads every file");
        }
        const Computer computer;
        QFile::setPermissions(computer.path("journal/" + machineId + "/system.journal"),
                              QFile::WriteOwner);
        TryReader reader(computer.setup());
        QCOMPARE(readAll(reader), TryReader::State::Unreadable);
        QVERIFY(computer.asked().isEmpty());
    }

    void noJournalAtAll() {
        const Computer computer;
        QFile::remove(computer.path("journal/" + machineId + "/system.journal"));
        TryReader reader(computer.setup());
        QCOMPARE(readAll(reader), TryReader::State::Failed);
        QFile::remove(computer.path("machine-id"));
        QCOMPARE(readAll(reader), TryReader::State::Failed);
    }

    void journalctlFails() {
        const Computer computer;
        computer.setExitCode(1);
        TryReader reader(computer.setup());
        QCOMPARE(readAll(reader), TryReader::State::Failed);
        QCOMPARE(computer.asked().size(), 1);
    }

    void noJournalctl() {
        const Computer computer;
        TryReader::Setup setup = computer.setup();
        setup.journalctl = computer.path("not-there");
        TryReader reader(setup);
        QCOMPARE(readAll(reader), TryReader::State::Failed);
    }

    // A child's flood of real tries, and a line too long to be an entry:
    // the page gets longer, it never fails.
    void aFloodAndAHugeLine() {
        const Computer computer;
        const qint64 start = 1793808120LL * 1000000;
        QByteArray lines;
        for (int i = 0; i < 20000; ++i) {
            lines += failedSudo(start + i * 1000, i + 1);
        }
        lines += QByteArray(200000, 'x') + '\n';
        lines += failedSudo(start + 20000 * 1000, 99999);
        computer.setStep("audit", lines);
        TryReader reader(computer.setup());
        QCOMPARE(readAll(reader), TryReader::State::Ready);
        QCOMPARE(reader.notices().shown.size(), 1U);
        QCOMPARE(reader.notices().shown[0].wrong, 20001);
    }

    // A second read while one runs is ignored; a read after it starts
    // afresh.
    void readAgain() {
        const Computer computer;
        computer.setStep("audit", failedSudo(1793808120LL * 1000000, 1));
        TryReader reader(computer.setup());
        QSignalSpy finished(&reader, &TryReader::finished);
        reader.read(FaillockSettings{});
        reader.read(FaillockSettings{});
        QVERIFY(finished.wait(60000));
        QTest::qWait(200);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(readAll(reader), TryReader::State::Ready);
        QCOMPARE(reader.notices().shown[0].wrong, 1);
    }
};

} // namespace

QTEST_GUILESS_MAIN(TryReaderTest)
#include "tst_tryreader.moc"
