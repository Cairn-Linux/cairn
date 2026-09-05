// SPDX-License-Identifier: Apache-2.0
#include "Fixture.h"
#include "Interpreter.h"

#include <QTest>

namespace {

QString firstLine(const Reply& reply) {
    return reply.lines.isEmpty() ? QString() : reply.lines.first().text;
}

QStringList texts(const Reply& reply) {
    QStringList out;
    for (const Reply::Line& line : reply.lines) {
        out.append(line.text);
    }
    return out;
}

class InterpreterTest : public QObject {
    Q_OBJECT

private slots:
    void startsAtTheRootWithFolders() {
        Interpreter shell(fixtureTree(), this);
        QCOMPARE(shell.location(), QStringLiteral("/"));
        const Reply reply = shell.run(QStringLiteral("ls"));
        QCOMPARE(texts(reply), (QStringList{QStringLiteral("make"), QStringLiteral("practice"),
                                            QStringLiteral("machine")}));
        QCOMPARE(reply.lines.first().icon, Reply::Icon::Folder);
        QVERIFY(!reply.launch);
    }

    void emptyLineSaysNothing() {
        Interpreter shell(fixtureTree(), this);
        QVERIFY(shell.run(QString()).lines.isEmpty());
        QVERIFY(shell.run(QStringLiteral("   ")).lines.isEmpty());
    }

    void cdGoesInAndLsShowsThingsWithTheirKind() {
        Interpreter shell(fixtureTree(), this);
        QVERIFY(shell.run(QStringLiteral("cd make")).lines.isEmpty());
        QCOMPARE(shell.location(), QStringLiteral("/make"));
        const Reply reply = shell.run(QStringLiteral("ls"));
        QCOMPARE(texts(reply), (QStringList{QStringLiteral("draw"), QStringLiteral("music"),
                                            QStringLiteral("tux-paint")}));
        QCOMPARE(reply.lines.first().icon, Reply::Icon::Make);
    }

    void lsCanPeekIntoAFolderFromTheRoot() {
        Interpreter shell(fixtureTree(), this);
        QCOMPARE(texts(shell.run(QStringLiteral("ls practice"))),
                 QStringList{QStringLiteral("practice")});
        QCOMPARE(shell.location(), QStringLiteral("/"));
    }

    void cdDotDotAndSlashAndBareCdReturnToTheRoot_data() {
        QTest::addColumn<QString>("line");
        QTest::newRow("dot dot") << "cd ..";
        QTest::newRow("slash") << "cd /";
        QTest::newRow("bare") << "cd";
    }

    void cdDotDotAndSlashAndBareCdReturnToTheRoot() {
        QFETCH(const QString, line);
        Interpreter shell(fixtureTree(), this);
        shell.run(QStringLiteral("cd machine"));
        QVERIFY(shell.run(line).lines.isEmpty());
        QCOMPARE(shell.location(), QStringLiteral("/"));
        // And at the root it is quietly fine.
        QVERIFY(shell.run(line).lines.isEmpty());
        QCOMPARE(shell.location(), QStringLiteral("/"));
    }

    void inputIsCaseInsensitive() {
        Interpreter shell(fixtureTree(), this);
        shell.run(QStringLiteral("CD Make"));
        QCOMPARE(shell.location(), QStringLiteral("/make"));
        QVERIFY(shell.run(QStringLiteral("OPEN Draw")).launch);
    }

    void openNamesTheProgramForTheHost() {
        Interpreter shell(fixtureTree(), this);
        shell.run(QStringLiteral("cd make"));
        const Reply reply = shell.run(QStringLiteral("open draw"));
        QCOMPARE(firstLine(reply), QStringLiteral("Opening Draw."));
        QVERIFY(reply.launch.has_value());
        const Reply::Launch launch = reply.launch.value_or(Reply::Launch{});
        QCOMPARE(launch.title, QStringLiteral("Draw"));
        QCOMPARE(launch.exec, QStringList{QStringLiteral("tuxpaint")});
    }

    void openWithNothingSetUpStillHandsOverAnEmptyExec() {
        // The host decides what an empty exec means (the grown-up screen).
        Interpreter shell(fixtureTree(), this);
        shell.run(QStringLiteral("cd make"));
        const Reply reply = shell.run(QStringLiteral("open music"));
        QVERIFY(reply.launch.has_value());
        QVERIFY(reply.launch.value_or(Reply::Launch{}).exec.isEmpty());
    }

    // Every wrong turn gets a next step. The first column is the state to
    // be in, then the line, then the sentence.
    void mistakesSuggest_data() {
        QTest::addColumn<QString>("before");
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("expected");
        QTest::newRow("unknown command, close")
            << "" << "opn draw" << "I don't know \"opn\". Did you mean open?";
        QTest::newRow("unknown command, far")
            << "" << "sudo ls" << "I don't know \"sudo\". Type help to see what I know.";
        QTest::newRow("rm is not close to anything")
            << "" << "rm" << "I don't know \"rm\". Type help to see what I know.";
        QTest::newRow("too many names")
            << "" << "open draw now" << "open takes one name at a time.";
        QTest::newRow("open needs a name")
            << "" << "open" << "open needs a name. Type ls to see them.";
        QTest::newRow("cat needs a name")
            << "" << "cat" << "cat needs a name. Type ls to see them.";
        QTest::newRow("open a folder")
            << "" << "open make" << "make is a place. Type cd make to go there.";
        QTest::newRow("cat a folder")
            << "" << "cat make" << "make is a place. Type cd make to go there.";
        QTest::newRow("thing in another folder, from root")
            << "" << "open draw" << "draw is in make. Type cd make first.";
        QTest::newRow("thing in another folder, from a folder")
            << "cd practice" << "open draw" << "draw is in make. Type cd .. and then cd make.";
        QTest::newRow("close name")
            << "cd make" << "open drw" << "I can't find \"drw\". Did you mean draw?";
        QTest::newRow("close folder")
            << "" << "cd mak" << "I can't find \"mak\". Did you mean make?";
        QTest::newRow("no such name")
            << "cd make" << "open freddi"
            << "I can't find \"freddi\" here. Type ls to see what is here.";
        QTest::newRow("cd into a thing")
            << "cd make" << "cd draw" << "draw is a thing to open, not a place. Type open draw.";
        QTest::newRow("ls a thing")
            << "cd make" << "ls draw" << "draw is a thing to open, not a place. Type open draw.";
        QTest::newRow("cat a thing")
            << "cd make" << "cat draw" << "draw is a thing to open, not to read. Type open draw.";
        QTest::newRow("cd sideways")
            << "cd make" << "cd practice" << "practice is next door. Type cd .. first.";
        QTest::newRow("help for an unknown")
            << "" << "help dir" << "I don't know \"dir\". Type help to see what I know.";
    }

    void mistakesSuggest() {
        QFETCH(const QString, before);
        QFETCH(const QString, line);
        QFETCH(const QString, expected);
        Interpreter shell(fixtureTree(), this);
        if (!before.isEmpty()) {
            shell.run(before);
        }
        const Reply reply = shell.run(line);
        QCOMPARE(texts(reply), QStringList{expected});
        QVERIFY(!reply.launch);
    }

    // Paths, programs and anything outside the tree are simply names the
    // shell cannot find. Nothing is ever run from here.
    void nothingOutsideTheTreeIsReachable_data() {
        QTest::addColumn<QString>("line");
        QTest::newRow("absolute path") << "open /usr/bin/xdg-open";
        QTest::newRow("relative path") << "open ../../bin/sh";
        QTest::newRow("home") << "cd ~";
        QTest::newRow("hidden") << "cat .bashrc";
        QTest::newRow("shell syntax") << "open draw;rm";
        QTest::newRow("option") << "ls -la";
        QTest::newRow("dot dot in a folder") << "cat ..";
    }

    void nothingOutsideTheTreeIsReachable() {
        QFETCH(const QString, line);
        Interpreter shell(fixtureTree(), this);
        shell.run(QStringLiteral("cd make"));
        const Reply reply = shell.run(line);
        QVERIFY(!reply.launch);
        QCOMPARE(reply.lines.size(), 1);
        QVERIFY(firstLine(reply).startsWith(QStringLiteral("I can't find")));
        QCOMPARE(shell.location(), QStringLiteral("/make"));
    }

    void helpListsTheFiveCommands() {
        Interpreter shell(fixtureTree(), this);
        const Reply all = shell.run(QStringLiteral("help"));
        QCOMPARE(all.lines.size(), 5);
        QCOMPARE(firstLine(all), QStringLiteral("ls shows what is here."));
        QCOMPARE(texts(shell.run(QStringLiteral("help open"))),
                 QStringList{QStringLiteral("open starts a thing.")});
    }

    void emptyTreeSaysSo() {
        Interpreter shell(AppTree({}), this);
        QCOMPARE(firstLine(shell.run(QStringLiteral("ls"))), QStringLiteral("Nothing here yet."));
        QCOMPARE(firstLine(shell.run(QStringLiteral("cd make"))),
                 QStringLiteral("I can't find \"make\" here. Type ls to see what is here."));
    }

    void completesCommandsThenNamesHere() {
        Interpreter shell(fixtureTree(), this);
        QCOMPARE(shell.complete(QString()), Interpreter::commands());
        QCOMPARE(shell.complete(QStringLiteral("c")),
                 (QStringList{QStringLiteral("cd"), QStringLiteral("cat")}));
        QCOMPARE(shell.complete(QStringLiteral("cd m")),
                 (QStringList{QStringLiteral("machine"), QStringLiteral("make")}));
        shell.run(QStringLiteral("cd make"));
        QCOMPARE(shell.complete(QStringLiteral("open ")),
                 (QStringList{QStringLiteral("draw"), QStringLiteral("music"),
                              QStringLiteral("tux-paint")}));
        QCOMPARE(shell.complete(QStringLiteral("open T")),
                 QStringList{QStringLiteral("tux-paint")});
        QVERIFY(shell.complete(QStringLiteral("open zzz")).isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(InterpreterTest)
#include "tst_interpreter.moc"
