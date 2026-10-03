#pragma once

#include "domain/types.h"

#include <QStringList>

namespace ktk {

struct CatalogFilterCriteria {
    QString query;
    QString manufacturer;
    QString cableType;
    QString insulation;
    QString cprClass;
    QString fireResistance;
};

class CatalogFilter final {
public:
    explicit CatalogFilter(CatalogFilterCriteria criteria);

    [[nodiscard]] bool matches(const CatalogItem &item) const;
    [[nodiscard]] static bool matches(
        const CatalogItem &item,
        const CatalogFilterCriteria &criteria);
    [[nodiscard]] static bool tokenizedMatch(
        const QString &query,
        const QString &text);
    [[nodiscard]] static QStringList queryTokens(const QString &text);

    [[nodiscard]] static QString cableFamily(const CatalogItem &item);
    [[nodiscard]] static QStringList insulationTags(const CableRow &cable);
    [[nodiscard]] static QStringList insulationTags(const CatalogItem &item);
    [[nodiscard]] static QStringList fireResistanceTags(const CatalogItem &item);

private:
    CatalogFilterCriteria m_criteria;
    QStringList m_queryTokens;
    QStringList m_cableTypeTokens;
};

} // namespace ktk
