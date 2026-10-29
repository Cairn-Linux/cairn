// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// The changes a parent has made on the Games page to the children's lists,
// kept for each child until a Save (ADR-0041). A parent can change two
// children and save once. Each child's list as saved is read from their own
// games file; a child with no change has no entry here.
class GameChoices {
public:
    // A child's list as their file holds it now, in its order. Set for each
    // child before any of their games is turned on or off.
    void setSaved(const QString& login, const QStringList& appIds);

    // The child's list as the parent wants it: their changes, or as saved.
    QStringList wanted(const QString& login) const;
    bool isWanted(const QString& login, const QString& appId) const;
    bool isSaved(const QString& login, const QString& appId) const;
    // Whether the child's list has a change that is not saved.
    bool changed(const QString& login) const;
    bool anyChanged() const;
    // The children among `logins` with a change, in that order.
    QStringList changedAmong(const QStringList& logins) const;

    // Puts a game on the child's list, or takes it off.
    void turn(const QString& login, const QString& appId);
    // The child's list goes back to how it is saved.
    void putBack(const QString& login);

    // cairn-family's arguments for a Save of every child with a change,
    // in the order of `logins`: save-games, then each child's whole new
    // list, such as ada=620,1001 or ben= for no games.
    // With no change, save-games alone, which changes no child's list.
    QStringList saveArguments(const QStringList& logins) const;

    // After the lists are read again: keeps changes only for the children in
    // `logins`, and only games still on their saved list or in `shared`,
    // since no other could show. A child whose wanted list is now the saved
    // one has no change left.
    void keepOnly(const QStringList& logins, const QSet<QString>& shared);

private:
    QHash<QString, QStringList> m_saved;
    QHash<QString, QStringList> m_wanted;
};
