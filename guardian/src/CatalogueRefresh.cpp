// SPDX-License-Identifier: Apache-2.0
#include "CatalogueRefresh.h"

#include "ReadableFile.h"
#include "SystemTool.h"

#include <QFile>
#include <QFileInfo>

namespace {

// kidscan's list of a thousand games is well under this.
constexpr qint64 largestList = qint64{16} * 1024 * 1024;

// The picture step reads the Guardian's Steam as root. It removes only the
// pictures of games neither shared nor on a child's list, and keeps a
// picture it cannot find again (provision/cairn-game-pictures.py).
bool makePictures(const CatalogueRefresh::Setup& setup, const QString& home, const QString& lists) {
    return SystemTool::run(setup.python,
                           {QStringLiteral("-I"), QStringLiteral("-B"), setup.pictureStep,
                            QStringLiteral("--home"), home, QStringLiteral("--shared"),
                            setup.library, QStringLiteral("--pictures"), setup.pictures,
                            QStringLiteral("--lists"), lists});
}

// With --library kidscan reads only the shared folder, never a home, and
// refuses a steamapps that is a link. Its list goes into the games file
// byte for byte, so provisioning's install_file still sees no change. It
// names no picture: a tile finds its game's picture by its app id
// (ADR-0047).
bool makeList(const CatalogueRefresh::Setup& setup) {
    const std::optional<QByteArray> list = SystemTool::output(
        setup.python,
        {QStringLiteral("-I"), QStringLiteral("-B"), setup.kidscan, QStringLiteral("--no-scummvm"),
         QStringLiteral("--library"), setup.library},
        largestList);
    if (!list || !GameCatalogue::fromJson(*list)) {
        return false;
    }
    const ReadableFile::Written written = ReadableFile::write(setup.gamesFile, *list);
    if (written == ReadableFile::Written::Changed && QFileInfo(setup.restorecon).isExecutable()) {
        SystemTool::run(setup.restorecon, {setup.gamesFile});
    }
    return written != ReadableFile::Written::Failed;
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
    result.picturesMade = home && makePictures(setup, *home, lists);
    result.catalogueMade = makeList(setup);
    result.catalogue = read(setup.gamesFile);
    return result;
}
