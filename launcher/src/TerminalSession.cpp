// SPDX-License-Identifier: Apache-2.0
#include "TerminalSession.h"

#include "HomeLayout.h"

#include <QDir>

namespace {

constexpr qsizetype keptHistory = 20;

World::Kind worldKind(TileModel::Kind kind) {
    switch (kind) {
    case TileModel::Kind::Make:
        return World::Kind::Make;
    case TileModel::Kind::Practice:
        return World::Kind::Practice;
    case TileModel::Kind::Machine:
        return World::Kind::Machine;
    }
    return World::Kind::Machine;
}

OutputModel::Icon outputIcon(Reply::Icon icon) {
    // The two enums list the same values in the same order.
    return static_cast<OutputModel::Icon>(icon);
}

} // namespace

TerminalSession::TerminalSession(QObject* parent)
    : QObject(parent), m_childName(QDir::home().dirName()), m_interpreter(World({}, {})),
      m_output(new OutputModel(this)) {}

TileModel* TerminalSession::tiles() const {
    return m_tiles;
}

void TerminalSession::setTiles(TileModel* tiles) {
    if (tiles == m_tiles) {
        return;
    }
    m_tiles = tiles;
    emit tilesChanged();
    reset();
}

QString TerminalSession::childName() const {
    return m_childName;
}

void TerminalSession::setChildName(const QString& name) {
    if (name == m_childName) {
        return;
    }
    m_childName = name;
    emit childNameChanged();
    reset();
}

QString TerminalSession::location() const {
    return m_interpreter.location();
}

OutputModel* TerminalSession::output() const {
    return m_output;
}

void TerminalSession::reset() {
    m_interpreter = Interpreter(worldFromTiles());
    m_output->clear();
    m_history.clear();
    emit locationChanged();
}

void TerminalSession::run(const QString& line) {
    const QString typed = line.trimmed();
    if (typed.isEmpty()) {
        return;
    }
    m_output->append({.text = location() + QStringLiteral(" > ") + typed,
                      .icon = OutputModel::Icon::None,
                      .isInput = true});
    m_history.append(typed);
    if (m_history.size() > keptHistory) {
        m_history.removeFirst();
    }

    const Reply reply = m_interpreter.run(typed);
    for (const Reply::Line& replyLine : reply.lines) {
        m_output->append(
            {.text = replyLine.text, .icon = outputIcon(replyLine.icon), .isInput = false});
    }
    emit locationChanged();
    if (reply.launch) {
        emit launchRequested(reply.launch->title, reply.launch->exec);
    }
    if (reply.leave) {
        emit left();
    }
}

QString TerminalSession::ghost(const QString& line) const {
    const QStringList candidates = m_interpreter.complete(line);
    if (candidates.isEmpty()) {
        return {};
    }
    const QString lastWord = line.section(QLatin1Char(' '), -1).toLower();
    return candidates.first().mid(lastWord.size());
}

QString TerminalSession::recall(int stepsBack) const {
    if (stepsBack < 1 || stepsBack > m_history.size()) {
        return {};
    }
    return m_history.at(m_history.size() - stepsBack);
}

World TerminalSession::worldFromTiles() const {
    QList<World::Door> doors;
    if (m_tiles != nullptr) {
        for (const TileModel::Tile& tile : m_tiles->tiles()) {
            if (!tile.opensTerminal) {
                doors.append(
                    {.title = tile.title, .kind = worldKind(tile.kind), .exec = tile.exec});
            }
        }
    }
    return {doors, HomeLayout::builtIn(m_childName)};
}
