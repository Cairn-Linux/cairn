// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "AccountKind.h"

#include <QQmlEngine>
#include <QSortFilterProxyModel>

#include <cstdint>
#include <functional>

// The tiles on the login screen (ADR-0020): SDDM's list of accounts, children
// first, then Guardians, then anyone outside the family, each group in the
// order the accounts were made. Nobody is left out. Each row gains what the
// tile draws: the kind, whether it asks for a password, the name to show,
// its first letter, and a colour.
class FamilyModel : public QSortFilterProxyModel {
    Q_OBJECT
    QML_ELEMENT

public:
    // A child's circle takes the next colour in turn, so siblings side by
    // side never match; everyone else gets the plain one.
    enum class Tint : std::uint8_t { Make, Practice, Machine, Plain };
    Q_ENUM(Tint)

    enum Role : std::uint16_t {
        KindRole = Qt::UserRole + 100,
        AsksForPasswordRole,
        DisplayNameRole,
        InitialRole,
        TintRole,
        LoginNameRole
    };

    Q_ENUM(Role)

    using LookUp = std::function<std::optional<Account>(const QString& name)>;

    explicit FamilyModel(QObject* parent = nullptr);

    // Tests give a table of accounts instead of the system's.
    void setLookUp(LookUp lookUp);

    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString sourceText(const QModelIndex& sourceIndex, const char* roleName) const;
    AccountKind::Kind kindOf(const QString& loginName) const;
    unsigned int uidOf(const QString& loginName) const;

    LookUp m_lookUp = lookUpAccount;
};
