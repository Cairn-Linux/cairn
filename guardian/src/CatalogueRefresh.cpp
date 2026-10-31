// SPDX-License-Identifier: Apache-2.0
#include "CatalogueRefresh.h"

#include "GamePicture.h"
#include "ReadableFile.h"
#include "SystemTool.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// kidscan's list of a thousand games is well under this.
constexpr qint64 largestList = qint64{16} * 1024 * 1024;
// The picture step says a few words a game at most.
constexpr qint64 largestPictureAnswer = qint64{1} << 20;

// The picture step reads the Guardian's Steam as root. It removes only the
// pictures of games neither shared nor on a child's list, and keeps a
// picture it cannot find again (provision/cairn-game-pictures.py).
void makePictures(CatalogueRefresh::Result& result, const CatalogueRefresh::Setup& setup,
                  const QString& home, const QString& lists) {
    const QStringList lines = SystemTool::lines(
        setup.python,
        {QStringLiteral("-I"), QStringLiteral("-B"), setup.pictureStep, QStringLiteral("--home"),
         home, QStringLiteral("--shared"), setup.library, QStringLiteral("--pictures"),
         setup.pictures, QStringLiteral("--lists"), lists},
        largestPictureAnswer);
    for (const QString& line : lines) {
        CatalogueRefresh::takePictureLine(result, line);
    }
    // So that the journal always says why the step did not finish: the
    // Save's sentence about the pictures sends the parent there.
    if (!result.picturesMade && !result.picturesEndedEarly) {
        result.pictureNotes.append(lines.isEmpty() ? QStringLiteral("no-answer")
                                                   : QStringLiteral("stopped"));
    }
}

// With --library kidscan reads only the shared folder, never a home, and
// refuses a steamapps that is a link. Its list goes into the games file
// byte for byte, so provisioning's install_file still sees no change. It
// names no picture: a tile finds its game's picture by its app id
// (ADR-0047). The answer is why the list is not in the file, or "".
QString makeList(const CatalogueRefresh::Setup& setup) {
    const std::optional<QByteArray> list = SystemTool::output(
        setup.python,
        {QStringLiteral("-I"), QStringLiteral("-B"), setup.kidscan, QStringLiteral("--no-scummvm"),
         QStringLiteral("--library"), setup.library},
        largestList);
    if (!list) {
        return QStringLiteral("failed");
    }
    if (!GameCatalogue::fromJson(*list)) {
        return QStringLiteral("not-a-games-list");
    }
    const ReadableFile::Written written = ReadableFile::write(setup.gamesFile, *list);
    if (written == ReadableFile::Written::Failed) {
        return QStringLiteral("not-written");
    }
    if (written == ReadableFile::Written::Changed && QFileInfo(setup.restorecon).isExecutable()) {
        SystemTool::run(setup.restorecon, {setup.gamesFile});
    }
    return {};
}

// The folder decides, as it does for the child's tile: a picture the step
// refused may have left an older one in place, and that one shows.
QStringList withoutPicture(const GameCatalogue& catalogue, const QString& pictures,
                           const QStringList& refused) {
    QStringList lines;
    for (const QJsonObject& entry : catalogue.entries()) {
        const QString appId = GameCatalogue::appIdOf(entry).value_or(QString());
        if (GamePicture::forAppId(pictures, appId).isEmpty()) {
            lines.append((refused.contains(appId) ? QStringLiteral("picture-refused ")
                                                  : QStringLiteral("picture-missing ")) +
                         appId);
        }
    }
    return lines;
}

} // namespace

std::optional<GameCatalogue> CatalogueRefresh::read(const QString& gamesFile) {
    QFile file(gamesFile);
    if (!file.open(QIODevice::ReadOnly) || file.size() > largestList) {
        return std::nullopt;
    }
    return GameCatalogue::fromJson(file.readAll());
}

CatalogueRefresh::Result CatalogueRefresh::run(const Setup& setup,
                                               const std::optional<QString>& home,
                                               const QString& lists) {
    Result result;
    if (home) {
        makePictures(result, setup, *home, lists);
    } else {
        result.pictureNotes.append(QStringLiteral("no-home"));
    }
    result.catalogueNote = makeList(setup);
    result.catalogueMade = result.catalogueNote.isEmpty();
    result.catalogue = read(setup.gamesFile);
    if (result.picturesMade && result.catalogue) {
        result.withoutPicture =
            withoutPicture(*result.catalogue, setup.pictures, result.picturesRefused);
    }
    return result;
}

// The step's own words (provision/cairn-game-pictures.py), with
// cairn-family's rules for an appid and a reason word.
void CatalogueRefresh::takePictureLine(Result& result, const QString& line) {
    static const QRegularExpression aGame(QRegularExpression::anchoredPattern(
        QStringLiteral("(copied|removed|missing) [1-9][0-9]{0,9}")));
    static const QRegularExpression leftOut(QRegularExpression::anchoredPattern(
        QStringLiteral("refused ([1-9][0-9]{0,9}) [a-z]+(?:-[a-z]+)*")));
    static const QRegularExpression ending(
        QRegularExpression::anchoredPattern(QStringLiteral("failed|refused folder|refused lists")));
    if (line == QLatin1String("finished")) {
        result.picturesMade = true;
    } else if (aGame.match(line).hasMatch()) {
        result.pictureNotes.append(line);
    } else if (const QRegularExpressionMatch match = leftOut.match(line); match.hasMatch()) {
        result.pictureNotes.append(line);
        result.picturesRefused.append(match.captured(1));
    } else if (ending.match(line).hasMatch()) {
        result.pictureNotes.append(line);
        result.picturesEndedEarly = true;
    }
}
