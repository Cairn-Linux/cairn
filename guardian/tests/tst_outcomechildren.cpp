// SPDX-License-Identifier: Apache-2.0
#include "OutcomeChildren.h"

#include <QTest>

namespace {

ListedChild ada() {
    return {.login = "ada", .name = "Ada", .fullName = "Ada", .level = 1, .language = {}};
}

ListedChild ben() {
    return {
        .login = "ben", .name = "Ben", .fullName = "Ben", .level = 2, .language = "en_GB.UTF-8"};
}

class OutcomeChildrenTest : public QObject {
    Q_OBJECT

private slots:
    // A sentence about nobody, such as "Type the child's name first.",
    // stays whatever happens to the family.
    void nobodyNeverChanges() {
        OutcomeChildren about;
        about.noteAsListed({ada(), ben()});
        QVERIFY(!about.changedIn({ada(), ben()}));
        QVERIFY(!about.changedIn({}));
    }

    // The same family, read again.
    void aChildAsTheyWereHasNotChanged() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.noteAsListed({ada(), ben()});
        QVERIFY(!about.changedIn({ada(), ben()}));
    }

    // "Ada can log in now" is not so once Ada's account is gone.
    void aChildGoneFromTheListHasChanged() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.noteAsListed({ada(), ben()});
        QVERIFY(about.changedIn({ben()}));
    }

    // "Ada is not a child's account" is so while she stays away, and not
    // once she is back.
    void aChildBackOnTheListHasChanged() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.noteAsListed({ben()});
        QVERIFY(!about.changedIn({ben()}));
        QVERIFY(!about.changedIn({}));
        QVERIFY(about.changedIn({ada(), ben()}));
    }

    // Anything their row shows.
    void aChildChangedOnTheListHasChanged_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("level");
        QTest::addColumn<QString>("language");
        QTest::addColumn<bool>("changed");

        QTest::newRow("as they were") << "Ben" << 2 << "en_GB.UTF-8" << false;
        QTest::newRow("another name") << "Benjamin" << 2 << "en_GB.UTF-8" << true;
        QTest::newRow("other capitals") << "BEN" << 2 << "en_GB.UTF-8" << true;
        QTest::newRow("another level") << "Ben" << 1 << "en_GB.UTF-8" << true;
        QTest::newRow("another language") << "Ben" << 2 << "en_US.UTF-8" << true;
        QTest::newRow("no language") << "Ben" << 2 << "" << true;
        QTest::newRow("a language Family does not offer") << "Ben" << 2 << "de_DE.UTF-8" << true;
    }

    void aChildChangedOnTheListHasChanged() {
        QFETCH(const QString, name);
        QFETCH(const int, level);
        QFETCH(const QString, language);
        QFETCH(const bool, changed);
        OutcomeChildren about;
        about.add(QStringLiteral("ben"));
        about.noteAsListed({ada(), ben()});
        const ListedChild now{
            .login = "ben", .name = name, .fullName = name, .level = level, .language = language};
        QCOMPARE(about.changedIn({ada(), now}), changed);
    }

    // What no row shows is no change: the row reads the same before and
    // after, so the sentence is as true as it was.
    void whatNoRowShowsIsNoChange() {
        OutcomeChildren about;
        about.add(QStringLiteral("cy"));
        const ListedChild before{
            .login = "cy", .name = "cy", .fullName = {}, .level = 1, .language = "de_DE.UTF-8"};
        about.noteAsListed({before});
        ListedChild now = before;
        now.fullName = QStringLiteral("cy");
        now.language = QString();
        QVERIFY(!about.changedIn({now}));
    }

    // Whatever else changed in the family, the sentence stays.
    void anotherChildsChangeIsNoChange() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.noteAsListed({ada(), ben()});
        ListedChild older = ben();
        older.level = 1;
        const ListedChild cy{
            .login = "cy", .name = "Cy", .fullName = "Cy", .level = 1, .language = {}};
        QVERIFY(!about.changedIn({ada()}));
        QVERIFY(!about.changedIn({ada(), older}));
        QVERIFY(!about.changedIn({cy, ada(), ben()}));
    }

    // "Another child is called Ben already", under Ada's new name: about
    // Ada, whose row it is, and about Ben, whom it names.
    void eitherOfTwoChildren() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.add(QStringLiteral("ben"));
        about.noteAsListed({ada(), ben()});
        QVERIFY(!about.changedIn({ada(), ben()}));
        QVERIFY(about.changedIn({ada()}));
        QVERIFY(about.changedIn({ben()}));
    }

    // Each sentence is noted over the list it was written from: the wait
    // for a password notes Ada at level 1, and the answer notes her at 2.
    void notedAgainWhenTheNextSentenceIsWritten() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.noteAsListed({ada()});
        ListedChild moved = ada();
        moved.level = 2;
        QVERIFY(about.changedIn({moved}));
        about.noteAsListed({moved});
        QVERIFY(!about.changedIn({moved}));
        QVERIFY(about.changedIn({ada()}));
    }

    // The next change starts with nobody.
    void clearedForTheNextChange() {
        OutcomeChildren about;
        about.add(QStringLiteral("ada"));
        about.noteAsListed({ada()});
        about.clear();
        QVERIFY(!about.changedIn({}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(OutcomeChildrenTest)
#include "tst_outcomechildren.moc"
