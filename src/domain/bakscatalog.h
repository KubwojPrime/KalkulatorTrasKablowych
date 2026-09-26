#pragma once

#include "domain/types.h"

#include <QString>
#include <QVector>

#include <optional>

namespace ktk {

class BaksCatalog final {
public:
    [[nodiscard]] static QVector<BaksProduct> load(
        QString *errorMessage = nullptr);
    [[nodiscard]] static BaksMassSummary summarize(
        const QVector<RouteAssemblyItem> &items);
    [[nodiscard]] static std::optional<BaksProduct> findById(
        const QVector<BaksProduct> &products,
        const QString &id);
    [[nodiscard]] static QString roleLabel(const QString &role);
    [[nodiscard]] static QString contributionLabel(
        const RouteAssemblyItem &item);
};

} // namespace ktk
