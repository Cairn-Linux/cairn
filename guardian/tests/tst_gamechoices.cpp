// SPDX-License-Identifier: Apache-2.0
#include "GameChoices.h"

#include <QTest>

namespace {

class GameChoicesTest : public QObject {
    Q_OBJECT

    static GameChoices family() {
        GameChoices choices;
        choices.setSaved("ada", {"1002"});
        choices.setSaved("ben", {});
        choices.setSaved("cy", {"1001", "2001"});
        return choices;
    }

private slots:
    void withNoChangeTheListIsTheSavedOne() {
        const GameChoices choices = family();
        QCOMPARE(choices.wanted("ada"), QStringList({"1002"}));
        QVERIFY(choices.isWanted("ada", "1002"));
        QVERIFY(choices.isSaved("ada", "1002"));
        QVERIFY(!choices.changed("ada"));
        QVERIFY(!choices.anyChanged());
    }

    void turningAGameOnAndOff() {
        GameChoices choices = family();
        choices.turn("ada", "1001");
        QCOMPARE(choices.wanted("ada"), QStringList({"1002", "1001"}));
        QVERIFY(choices.changed("ada"));
        QVERIFY(!choices.isSaved("ada", "1001"));
        choices.turn("ada", "1002");
        QCOMPARE(choices.wanted("ada"), QStringList({"1001"}));
        QVERIFY(choices.anyChanged());
    }

    // Turning a game back leaves no change, whatever the order.
    void turningBackLeavesNoChange() {
        GameChoices choices = family();
        choices.turn("cy", "1001");
        choices.turn("cy", "1001");
        QVERIFY(!choices.changed("cy"));
        choices.turn("cy", "2001");
        choices.turn("cy", "1001");
        choices.turn("cy", "2001");
        choices.turn("cy", "1001");
        QVERIFY(!choices.changed("cy"));
        QVERIFY(!choices.anyChanged());
    }

    // Each child's changes are their own.
    void twoChildrenAtOnce() {
        GameChoices choices = family();
        choices.turn("ada", "1001");
        choices.turn("ben", "1003");
        QVERIFY(choices.changed("ada"));
        QVERIFY(choices.changed("ben"));
        QVERIFY(!choices.changed("cy"));
        choices.putBack("ada");
        QVERIFY(!choices.changed("ada"));
        QCOMPARE(choices.wanted("ada"), QStringList({"1002"}));
        QVERIFY(choices.changed("ben"));
    }

    // Each changed child's whole list, in the children's order, and an
    // empty list as nothing after the =.
    void theSaveArguments() {
        GameChoices choices = family();
        choices.turn("ben", "1003");
        choices.turn("ben", "1001");
        choices.turn("ada", "1002");
        QCOMPARE(choices.saveArguments({"ada", "ben", "cy"}),
                 QStringList({"save-games", "ada=", "ben=1003,1001"}));
    }

    // Every --log-out comes first, in the children's order, and only for a
    // child whose list the Save gives.
    void theChildrenToLogOutComeFirst() {
        GameChoices choices = family();
        choices.turn("ben", "1003");
        choices.turn("ada", "1001");
        QCOMPARE(choices.saveArguments({"ada", "ben", "cy"}, {"ben", "cy", "ada"}),
                 QStringList({"save-games", "--log-out=ada", "--log-out=ben", "ada=1002,1001",
                              "ben=1003"}));
        QCOMPARE(family().saveArguments({"ada", "ben"}, {"ada"}), QStringList({"save-games"}));
    }

    void noChangeGivesOnlyTheSubcommand() {
        QCOMPARE(family().saveArguments({"ada", "ben"}), QStringList({"save-games"}));
    }

    // After the lists are read again, a change now saved is gone, and so
    // is a child no longer there.
    void keepOnlyWhatIsStillAChange() {
        GameChoices choices = family();
        choices.turn("ada", "1001");
        choices.turn("ben", "1003");
        choices.turn("cy", "1003");
        choices.setSaved("ada", {"1002", "1001"});
        choices.keepOnly({"ada", "ben"}, {"1001", "1002", "1003"});
        QVERIFY(!choices.changed("ada"));
        QVERIFY(choices.changed("ben"));
        QVERIFY(!choices.changed("cy"));
    }

    // A game neither shared nor on the saved list can no longer show, so it
    // is dropped; one still on the saved list stays.
    void keepOnlyGamesThatCanShow() {
        GameChoices choices = family();
        choices.turn("cy", "1003");
        choices.turn("ben", "1003");
        choices.turn("ben", "1004");
        choices.keepOnly({"ada", "ben", "cy"}, {"1001", "1004"});
        QVERIFY(!choices.changed("cy"));
        QCOMPARE(choices.wanted("cy"), QStringList({"1001", "2001"}));
        QCOMPARE(choices.wanted("ben"), QStringList({"1004"}));
    }

    // A game no longer shared that is on the saved list can be taken off.
    void aGameNoLongerSharedCanBeTakenOff() {
        GameChoices choices = family();
        choices.turn("cy", "2001");
        choices.keepOnly({"cy"}, {"1001"});
        QCOMPARE(choices.wanted("cy"), QStringList({"1001"}));
        QCOMPARE(choices.saveArguments({"cy"}), QStringList({"save-games", "cy=1001"}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(GameChoicesTest)
#include "tst_gamechoices.moc"
