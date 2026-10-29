// SPDX-License-Identifier: Apache-2.0
#include "GameChoices.h"
#include "LogOutQuestion.h"
#include "WaitingGames.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

QByteArray gamesFile(const QStringList& appIds) {
    QJsonArray entries;
    for (const QString& appId : appIds) {
        entries.append(QJsonObject{{"id", "steam-" + appId},
                                   {"title", "Game " + appId},
                                   {"category", "games"},
                                   {"exec", QJsonArray{"steam", "-applaunch", appId}}});
    }
    return QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}}).toJson();
}

std::vector<Account> family() {
    return {
        {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"},
        {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"},
        {.uid = 1003, .groups = {"dee", "cairn-l1"}, .login = "dee", .fullName = "Dee"},
    };
}

class LogOutQuestionTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    static void write(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    // The children to ask about, as logins, with `loggedIn` logged in, and
    // the choices the page holds once each child's lists are read.
    QStringList asked(const QSet<QString>& loggedIn) {
        WaitingGames lists({.childLists = path("games"),
                            .waiting = path("games-waiting"),
                            .finished = path("games-finished"),
                            .owner = getuid()});
        lists.read(family());
        const std::vector<ListedChild> children = childrenAmong(family());
        for (const ListedChild& child : children) {
            m_choices.setSaved(child.login, lists.baseline(child.login));
        }
        for (const auto& [login, appId] : m_turns) {
            m_choices.turn(login, appId);
        }
        m_askedLogind = 0;
        QStringList logins;
        const auto who =
            LogOutQuestion::whoToAsk(children, m_choices, lists, m_shared, [this, loggedIn] {
                ++m_askedLogind;
                return loggedIn;
            });
        for (const ListedChild& child : who) {
            logins.append(child.login);
        }
        return logins;
    }

    static std::vector<ListedChild> adaAndBen() {
        return {{.login = "ada", .name = "Ada", .level = 1},
                {.login = "ben", .name = "Ben", .level = 2}};
    }

    QTemporaryDir m_temporary;
    GameChoices m_choices;
    QList<std::pair<QString, QString>> m_turns;
    QSet<QString> m_shared;
    int m_askedLogind = 0;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        for (const char* folder : {"games", "games-waiting", "games-finished"}) {
            QDir(path(folder)).removeRecursively();
            QVERIFY(QDir().mkpath(path(folder)));
        }
        write(path("games/ada.json"), gamesFile({"1001"}));
        write(path("games/ben.json"), gamesFile({}));
        write(path("games/dee.json"), gamesFile({"1001", "1002"}));
        m_choices = GameChoices();
        m_turns.clear();
        m_shared = {"1001", "1002", "1003"};
    }

    // A child with a new game to copy who is logged in, and only them.
    void asksAboutALoggedInChildWithGamesToCopy() {
        m_turns = {{"ada", "1002"}, {"ben", "1003"}};
        QCOMPARE(asked({"ada", "guardian"}), QStringList({"ada"}));
        QCOMPARE(m_askedLogind, 1);
    }

    // In the children's order, not the order they were changed.
    void inTheChildrensOrder() {
        m_turns = {{"ben", "1003"}, {"ada", "1002"}};
        QCOMPARE(asked({"ben", "ada"}), QStringList({"ada", "ben"}));
    }

    void nobodyLoggedIn() {
        m_turns = {{"ada", "1002"}};
        QVERIFY(asked({}).isEmpty());
        QCOMPARE(m_askedLogind, 1);
    }

    // Taking games off copies nothing, so nobody is asked about, and logind
    // is not asked.
    void takingGamesOffAsksNothing() {
        m_turns = {{"ada", "1001"}, {"dee", "1002"}};
        QVERIFY(asked({"ada", "dee"}).isEmpty());
        QCOMPARE(m_askedLogind, 0);
    }

    // A game the shared folder no longer has cannot be copied.
    void aGameNotSharedIsNoCopy() {
        m_shared = {"1001"};
        m_turns = {{"ada", "1002"}};
        QVERIFY(asked({"ada"}).isEmpty());
        QCOMPARE(m_askedLogind, 0);
    }

    // No change, no question, even with games waiting.
    void noChangeNoQuestion() {
        write(path("games-waiting/ada"), "ada=1001,1002\n");
        QVERIFY(asked({"ada"}).isEmpty());
    }

    // A game that only waits for the child's logout is still a game to
    // copy, whatever else the parent changes.
    void aWaitingGameIsStillToCopy() {
        write(path("games-waiting/ada"), "ada=1001,1002\n");
        m_turns = {{"ada", "1001"}};
        QCOMPARE(asked({"ada"}), QStringList({"ada"}));
    }

    // With nobody to ask about, it is decided at once, logging nobody out,
    // and no question opens.
    void nobodyToAsk() {
        LogOutQuestion question;
        const QSignalSpy changed(&question, &LogOutQuestion::changed);
        const QSignalSpy decided(&question, &LogOutQuestion::decided);
        question.ask({});
        QCOMPARE(decided.size(), 1);
        QVERIFY(decided.constFirst().constFirst().toStringList().isEmpty());
        QVERIFY(changed.isEmpty());
        QCOMPARE(question.step(), LogOutQuestion::Step::NotAsking);
    }

    // One child at a time: the choice, the warning, back, then on.
    void eachChildInTurn() {
        LogOutQuestion question;
        const QSignalSpy decided(&question, &LogOutQuestion::decided);
        question.ask(adaAndBen());
        QVERIFY(question.asking());
        QCOMPARE(question.step(), LogOutQuestion::Step::Choosing);
        QCOMPARE(question.login(), QStringLiteral("ada"));
        QCOMPARE(question.name(), QStringLiteral("Ada"));

        question.later();
        QCOMPARE(question.step(), LogOutQuestion::Step::Choosing);
        QCOMPARE(question.name(), QStringLiteral("Ben"));
        question.logOutNow();
        QCOMPARE(question.step(), LogOutQuestion::Step::Warning);
        QCOMPARE(question.name(), QStringLiteral("Ben"));
        question.notNow();
        QCOMPARE(question.step(), LogOutQuestion::Step::Choosing);
        QCOMPARE(question.name(), QStringLiteral("Ben"));
        QVERIFY(decided.isEmpty());

        question.logOutNow();
        question.logOut();
        QCOMPARE(decided.size(), 1);
        QCOMPARE(decided.constFirst().constFirst().toStringList(), QStringList({"ben"}));
        QCOMPARE(question.step(), LogOutQuestion::Step::NotAsking);
        QCOMPARE(question.login(), QString());
        QCOMPARE(question.name(), QString());
    }

    // The question has closed by the time the Save starts.
    void closesBeforeItIsDecided() {
        LogOutQuestion question;
        LogOutQuestion::Step stepWhenDecided = LogOutQuestion::Step::Warning;
        connect(&question, &LogOutQuestion::decided, this,
                [&question, &stepWhenDecided] { stepWhenDecided = question.step(); });
        question.ask({{.login = "ada", .name = "Ada", .level = 1}});
        question.logOutNow();
        question.logOut();
        QCOMPARE(stepWhenDecided, LogOutQuestion::Step::NotAsking);
    }

    void bothLoggedOutInTheOrderAsked() {
        LogOutQuestion question;
        const QSignalSpy decided(&question, &LogOutQuestion::decided);
        question.ask(adaAndBen());
        for (int i = 0; i < 2; ++i) {
            question.logOutNow();
            question.logOut();
        }
        QCOMPARE(decided.size(), 1);
        QCOMPARE(decided.constFirst().constFirst().toStringList(), QStringList({"ada", "ben"}));
    }

    // A button of a step not shown does nothing: no child is ever logged out
    // without the warning.
    void onlyTheStepShownAnswers() {
        LogOutQuestion question;
        const QSignalSpy changed(&question, &LogOutQuestion::changed);
        const QSignalSpy decided(&question, &LogOutQuestion::decided);
        question.logOutNow();
        question.later();
        question.logOut();
        question.notNow();
        QVERIFY(changed.isEmpty());
        QVERIFY(decided.isEmpty());

        question.ask(adaAndBen());
        question.logOut();
        question.notNow();
        QCOMPARE(question.step(), LogOutQuestion::Step::Choosing);
        QCOMPARE(question.login(), QStringLiteral("ada"));
        question.logOutNow();
        question.later();
        question.logOutNow();
        QCOMPARE(question.step(), LogOutQuestion::Step::Warning);
        QCOMPARE(question.login(), QStringLiteral("ada"));
        QVERIFY(decided.isEmpty());
    }

    // A second Save pressed while the question is open changes nothing.
    void askingAgainWhileAskingDoesNothing() {
        LogOutQuestion question;
        const QSignalSpy decided(&question, &LogOutQuestion::decided);
        question.ask(adaAndBen());
        question.ask({});
        question.ask({{.login = "dee", .name = "Dee", .level = 1}});
        QVERIFY(decided.isEmpty());
        QCOMPARE(question.login(), QStringLiteral("ada"));
        question.later();
        QCOMPARE(question.login(), QStringLiteral("ben"));
    }

    // A new question starts with nobody to log out.
    void eachQuestionStartsAfresh() {
        LogOutQuestion question;
        const QSignalSpy decided(&question, &LogOutQuestion::decided);
        question.ask(adaAndBen());
        question.logOutNow();
        question.logOut();
        question.later();
        question.ask(adaAndBen());
        question.later();
        question.later();
        QCOMPARE(decided.size(), 2);
        QCOMPARE(decided.at(0).constFirst().toStringList(), QStringList({"ada"}));
        QVERIFY(decided.at(1).constFirst().toStringList().isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(LogOutQuestionTest)
#include "tst_logoutquestion.moc"
