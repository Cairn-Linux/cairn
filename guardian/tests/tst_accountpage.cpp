// SPDX-License-Identifier: Apache-2.0
#include "AccountPage.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <memory>

namespace {

constexpr int kAnswerMilliseconds = 10000;
constexpr auto userPath = QLatin1StringView("/org/freedesktop/Accounts/User1000");

// What the stand-in AccountsService was asked, shared with its thread.
struct Asked {
    QMutex mutex;
    QString iconFile;
    QStringList given;
    QImage lastPicture;
    bool refuse = false;
};

// An error answer to a call, as AccountsService gives one. Qt passes the
// call itself to a method whose last parameter is a QDBusMessage; it is not
// part of the method's D-Bus signature.
void refuse(const QDBusMessage& call, const QString& text) {
    call.setDelayedReply(true);
    QDBusConnection(QStringLiteral("accounts"))
        .send(call.createErrorReply(QStringLiteral("org.freedesktop.Accounts.Error.Failed"), text));
}

// AccountsService's own object: it knows only the account 1000.
class StandInAccounts : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts")

public slots:
    QDBusObjectPath FindUserById(qlonglong uid, const QDBusMessage& call) {
        if (uid != 1000) {
            refuse(call, QStringLiteral("Failed to look up user with uid %1.").arg(uid));
            return {};
        }
        return QDBusObjectPath(userPath);
    }
};

// The account's object: its picture, and SetIconFile, which keeps a copy
// of the file as AccountsService does, or refuses when told to.
class StandInUser : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts.User")
    Q_PROPERTY(QString IconFile READ iconFile)

public:
    StandInUser(Asked* asked, QString copy) : m_asked(asked), m_copy(std::move(copy)) {}

    QString iconFile() const {
        const QMutexLocker lock(&m_asked->mutex);
        return m_asked->iconFile;
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): Qt's slots.
public slots:
    void SetIconFile(const QString& file, const QDBusMessage& call) {
        const QMutexLocker lock(&m_asked->mutex);
        m_asked->given.append(file);
        if (m_asked->refuse) {
            refuse(call, QStringLiteral("file is too large to be used as an image"));
            return;
        }
        m_asked->lastPicture = QImage(file);
        QFile::remove(m_copy);
        QFile::copy(file, m_copy);
        m_asked->iconFile = m_copy;
    }

private:
    Asked* m_asked;
    QString m_copy;
};

class AccountPageTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_files;
    QThread m_thread;
    Asked m_asked;
    std::unique_ptr<QDBusConnection> m_serviceBus;

    QString file(const QString& name) const {
        return QDir(m_files.path()).filePath(name);
    }

    std::unique_ptr<AccountPage> page(bool withBus = true, unsigned int uid = 1000) {
        AccountPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        setup.uid = uid;
        setup.lookUpUid = [](unsigned int asked) -> std::optional<Account> {
            if (asked != 1000) {
                return std::nullopt;
            }
            return Account{.uid = 1000,
                           .groups = {"guardian", "wheel", "cairn-guardian"},
                           .login = "guardian",
                           .fullName = "Gina Guardian"};
        };
        if (withBus) {
            setup.bus = QDBusConnection::sessionBus();
        }
        setup.stockPictures = file(QStringLiteral("avatars"));
        return std::make_unique<AccountPage>(setup);
    }

    // The new password the stand-in cairn-family read, and its arguments.
    struct PasswordRun {
        AccountPage::Step step;
        AccountPage::Reason reason;
        QByteArray input;
        QByteArray arguments;
    };

    PasswordRun changePassword(const QString& scenario, const QString& password,
                               const QString& again) {
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        qputenv("CAIRN_TEST_INPUT", file(QStringLiteral("input")).toLocal8Bit());
        qputenv("CAIRN_TEST_ARGUMENTS", file(QStringLiteral("arguments")).toLocal8Bit());
        QFile::remove(file(QStringLiteral("input")));
        QFile::remove(file(QStringLiteral("arguments")));
        const std::unique_ptr<AccountPage> account = page();
        account->changePassword(password, again);
        if (account->step() == AccountPage::Step::Asking) {
            static_cast<void>(
                QTest::qWaitFor([&account] { return !account->busy(); }, kAnswerMilliseconds));
        }
        qunsetenv("CAIRN_TEST_INPUT");
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QFile input(file(QStringLiteral("input")));
        QFile arguments(file(QStringLiteral("arguments")));
        return {.step = account->step(),
                .reason = account->reason(),
                .input = input.open(QIODevice::ReadOnly) ? input.readAll() : QByteArray(),
                .arguments =
                    arguments.open(QIODevice::ReadOnly) ? arguments.readAll() : QByteArray()};
    }

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void namesTheGrownUp();
    void listsPlasmasPictures();
    void showsThePictureItHasNow();
    void aChosenPictureGoesToAccountsService();
    void aPictureItCannotRead();
    void aPictureAccountsServiceRefuses();
    void noBusOrNoAccount();

    void checksTheNewPasswordFirst_data();
    void checksTheNewPasswordFirst();
    void aNewPasswordGoesOnStandardInput();
    void everyAnswer_data();
    void everyAnswer();
    void oneChangeAtATime();
};

void AccountPageTest::initTestCase() {
    QVERIFY2(QDBusConnection::sessionBus().isConnected(),
             "run under dbus-run-session, as CTest does");
    QVERIFY(m_files.isValid());
    QVERIFY(QDir(m_files.path()).mkpath(QStringLiteral("avatars/photos")));
    for (const QString& name : {QStringLiteral("Konqi"), QStringLiteral("Artist Konqi")}) {
        QImage image(300, 200, QImage::Format_ARGB32);
        image.fill(Qt::darkGreen);
        QVERIFY(image.save(file(QStringLiteral("avatars/%1.png").arg(name))));
    }
    QImage photo(120, 80, QImage::Format_RGB32);
    photo.fill(Qt::darkBlue);
    QVERIFY(photo.save(file(QStringLiteral("avatars/bill.jpg"))));
    QFile notes(file(QStringLiteral("avatars/notes.txt")));
    QVERIFY(notes.open(QIODevice::WriteOnly));
    notes.close();

    // The stand-in answers from a thread of its own, so the page's calls,
    // which wait for their answer, get one.
    m_serviceBus = std::make_unique<QDBusConnection>(
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("accounts")));
    auto* accounts = new StandInAccounts;
    auto* user = new StandInUser(&m_asked, file(QStringLiteral("icon-copy")));
    accounts->moveToThread(&m_thread);
    user->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, accounts, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished, user, &QObject::deleteLater);
    m_thread.start();
    QVERIFY(m_serviceBus->registerObject(QStringLiteral("/org/freedesktop/Accounts"), accounts,
                                         QDBusConnection::ExportAllSlots));
    QVERIFY(m_serviceBus->registerObject(
        userPath, user, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties));
    QVERIFY(m_serviceBus->registerService(QStringLiteral("org.freedesktop.Accounts")));
}

void AccountPageTest::cleanupTestCase() {
    QDBusConnection::disconnectFromBus(m_serviceBus->name());
    m_thread.quit();
    m_thread.wait();
}

void AccountPageTest::init() {
    const QMutexLocker lock(&m_asked.mutex);
    m_asked.iconFile.clear();
    m_asked.given.clear();
    m_asked.lastPicture = QImage();
    m_asked.refuse = false;
}

void AccountPageTest::namesTheGrownUp() {
    QCOMPARE(page()->name(), QStringLiteral("Gina Guardian"));
    QCOMPARE(page()->step(), AccountPage::Step::Ready);
    QVERIFY(!page()->busy());
}

// Only the pictures, PNG and JPEG, by name, without their ending; never a
// folder or another file.
void AccountPageTest::listsPlasmasPictures() {
    const QVariantList pictures = page()->pictures();
    QCOMPARE(pictures.size(), 3);
    QCOMPARE(pictures.at(0).toMap().value("name").toString(), QStringLiteral("Artist Konqi"));
    QCOMPARE(pictures.at(0).toMap().value("url").toUrl(),
             QUrl::fromLocalFile(file(QStringLiteral("avatars/Artist Konqi.png"))));
    // In name order, whatever the case, as Plasma's own folder mixes both.
    QCOMPARE(pictures.at(1).toMap().value("name").toString(), QStringLiteral("bill"));
    QCOMPARE(pictures.at(2).toMap().value("name").toString(), QStringLiteral("Konqi"));
}

void AccountPageTest::showsThePictureItHasNow() {
    QVERIFY(page()->picture().isEmpty());
    {
        const QMutexLocker lock(&m_asked.mutex);
        m_asked.iconFile = file(QStringLiteral("avatars/Konqi.png"));
    }
    const QUrl shown = page()->picture();
    QCOMPARE(shown.toLocalFile(), file(QStringLiteral("avatars/Konqi.png")));
    QVERIFY(!shown.query().isEmpty());
}

// The page makes the picture small and square first, and shows the new
// one under a new URL, so the old one is not kept.
void AccountPageTest::aChosenPictureGoesToAccountsService() {
    const std::unique_ptr<AccountPage> account = page();
    const QSignalSpy pictureChanged(account.get(), &AccountPage::pictureChanged);
    const QUrl before = account->picture();
    account->choosePicture(QUrl::fromLocalFile(file(QStringLiteral("avatars/Konqi.png"))));
    QCOMPARE(account->step(), AccountPage::Step::SettingPicture);
    QVERIFY(account->busy());
    QTRY_COMPARE_WITH_TIMEOUT(account->step(), AccountPage::Step::PictureChanged,
                              kAnswerMilliseconds);
    QVERIFY(!account->busy());
    QCOMPARE(account->reason(), AccountPage::Reason::None);
    {
        const QMutexLocker lock(&m_asked.mutex);
        QCOMPARE(m_asked.given.size(), 1);
        QVERIFY(m_asked.given.constFirst() != file(QStringLiteral("avatars/Konqi.png")));
        QCOMPARE(m_asked.lastPicture.size(), QSize(200, 200));
    }
    QCOMPARE(account->picture().toLocalFile(), file(QStringLiteral("icon-copy")));
    QVERIFY(account->picture() != before);
    QVERIFY(!pictureChanged.isEmpty());

    // The same picture again is a new URL again.
    const QUrl first = account->picture();
    account->choosePicture(QUrl::fromLocalFile(file(QStringLiteral("avatars/Konqi.png"))));
    QTRY_VERIFY_WITH_TIMEOUT(!account->busy(), kAnswerMilliseconds);
    QVERIFY(account->picture() != first);
}

void AccountPageTest::aPictureItCannotRead() {
    const std::unique_ptr<AccountPage> account = page();
    for (const QUrl& url : {QUrl::fromLocalFile(file(QStringLiteral("avatars/notes.txt"))),
                            QUrl::fromLocalFile(file(QStringLiteral("missing.png"))),
                            QUrl(QStringLiteral("https://example.com/a.png")), QUrl()}) {
        account->choosePicture(url);
        QCOMPARE(account->step(), AccountPage::Step::PictureNotChanged);
        QCOMPARE(account->reason(), AccountPage::Reason::PictureUnreadable);
    }
    const QMutexLocker lock(&m_asked.mutex);
    QVERIFY(m_asked.given.isEmpty());
}

void AccountPageTest::aPictureAccountsServiceRefuses() {
    {
        const QMutexLocker lock(&m_asked.mutex);
        m_asked.refuse = true;
    }
    const std::unique_ptr<AccountPage> account = page();
    account->choosePicture(QUrl::fromLocalFile(file(QStringLiteral("avatars/Konqi.png"))));
    QTRY_COMPARE_WITH_TIMEOUT(account->step(), AccountPage::Step::PictureNotChanged,
                              kAnswerMilliseconds);
    QCOMPARE(account->reason(), AccountPage::Reason::PictureRefused);
    QVERIFY(account->picture().isEmpty());
}

void AccountPageTest::noBusOrNoAccount() {
    const std::unique_ptr<AccountPage> noBus = page(false);
    QVERIFY(noBus->picture().isEmpty());
    noBus->choosePicture(QUrl::fromLocalFile(file(QStringLiteral("avatars/Konqi.png"))));
    QCOMPARE(noBus->step(), AccountPage::Step::PictureNotChanged);
    QCOMPARE(noBus->reason(), AccountPage::Reason::PictureRefused);

    const std::unique_ptr<AccountPage> stranger = page(true, 4242);
    QCOMPARE(stranger->name(), QString());
    QVERIFY(stranger->picture().isEmpty());
    stranger->choosePicture(QUrl::fromLocalFile(file(QStringLiteral("avatars/Konqi.png"))));
    QCOMPARE(stranger->step(), AccountPage::Step::PictureNotChanged);
    QCOMPARE(stranger->reason(), AccountPage::Reason::PictureRefused);
    const QMutexLocker lock(&m_asked.mutex);
    QVERIFY(m_asked.given.isEmpty());
}

// No password box opens for a new password that would not be taken.
void AccountPageTest::checksTheNewPasswordFirst_data() {
    QTest::addColumn<QString>("password");
    QTest::addColumn<QString>("again");
    QTest::addColumn<AccountPage::Reason>("reason");

    QTest::newRow("empty") << "" << "" << AccountPage::Reason::Empty;
    QTest::newRow("empty, the second typed") << "" << "new" << AccountPage::Reason::Empty;
    QTest::newRow("different") << "new" << "newt" << AccountPage::Reason::Different;
    QTest::newRow("the second empty") << "new" << "" << AccountPage::Reason::Different;
    QTest::newRow("too long") << QString(129, QLatin1Char('a')) << QString(129, QLatin1Char('a'))
                              << AccountPage::Reason::TooLong;
    QTest::newRow("a tab") << "a\tb" << "a\tb" << AccountPage::Reason::Unprintable;
}

void AccountPageTest::checksTheNewPasswordFirst() {
    QFETCH(const QString, password);
    QFETCH(const QString, again);
    QFETCH(const AccountPage::Reason, reason);
    const PasswordRun run = changePassword(QStringLiteral("done"), password, again);
    QCOMPARE(run.step, AccountPage::Step::PasswordNotChanged);
    QCOMPARE(run.reason, reason);
    QVERIFY(run.arguments.isEmpty());
}

// The new password is the command's standard input and never one of its
// arguments, which every account could read while it runs.
void AccountPageTest::aNewPasswordGoesOnStandardInput() {
    const QString password = QStringLiteral("Zoë's new: pass $HOME");
    const PasswordRun run = changePassword(QStringLiteral("done"), password, password);
    QCOMPARE(run.step, AccountPage::Step::PasswordChanged);
    QCOMPARE(run.reason, AccountPage::Reason::None);
    QCOMPARE(run.arguments, QByteArrayLiteral("run\nset-own-password\n"));
    QCOMPARE(run.input, password.toUtf8() + '\n');
}

void AccountPageTest::everyAnswer_data() {
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<AccountPage::Reason>("reason");

    QTest::newRow("not-root") << "refused-not-root" << AccountPage::Reason::NotRoot;
    QTest::newRow("not-a-grown-up") << "refused-not-a-grown-up" << AccountPage::Reason::NotAGrownUp;
    QTest::newRow("usage") << "refused-usage" << AccountPage::Reason::Usage;
    QTest::newRow("bad-password") << "refused-bad-password" << AccountPage::Reason::BadPassword;
    QTest::newRow("failed") << "refused-failed" << AccountPage::Reason::Failed;
    QTest::newRow("a word it does not know") << "refused-taken" << AccountPage::Reason::Unknown;
    QTest::newRow("box closed") << "dismissed" << AccountPage::Reason::Dismissed;
    QTest::newRow("wrong password") << "not-authorized" << AccountPage::Reason::NotAuthorized;
    QTest::newRow("garbage") << "garbage" << AccountPage::Reason::Unknown;
    QTest::newRow("crashed") << "crashes" << AccountPage::Reason::Unknown;
}

void AccountPageTest::everyAnswer() {
    QFETCH(const QString, scenario);
    QFETCH(const AccountPage::Reason, reason);
    const PasswordRun run = changePassword(scenario, QStringLiteral("new"), QStringLiteral("new"));
    QCOMPARE(run.step, AccountPage::Step::PasswordNotChanged);
    QCOMPARE(run.reason, reason);
}

// While the password box is open, neither a second password nor a picture
// starts.
void AccountPageTest::oneChangeAtATime() {
    qputenv("CAIRN_TEST_SCENARIO", "asking");
    qputenv("CAIRN_TEST_ARGUMENTS", file(QStringLiteral("arguments")).toLocal8Bit());
    QFile::remove(file(QStringLiteral("arguments")));
    const std::unique_ptr<AccountPage> account = page();
    account->changePassword(QStringLiteral("new"), QStringLiteral("new"));
    QCOMPARE(account->step(), AccountPage::Step::Asking);
    QVERIFY(account->busy());
    account->changePassword(QStringLiteral("other"), QStringLiteral("other"));
    account->choosePicture(QUrl::fromLocalFile(file(QStringLiteral("avatars/Konqi.png"))));
    QCOMPARE(account->step(), AccountPage::Step::Asking);
    QTest::qWait(300);
    qunsetenv("CAIRN_TEST_ARGUMENTS");
    QFile arguments(file(QStringLiteral("arguments")));
    QVERIFY(arguments.open(QIODevice::ReadOnly));
    QCOMPARE(arguments.readAll().count("run\n"), 1);
    const QMutexLocker lock(&m_asked.mutex);
    QVERIFY(m_asked.given.isEmpty());
}

} // namespace

QTEST_MAIN(AccountPageTest)
#include "tst_accountpage.moc"
