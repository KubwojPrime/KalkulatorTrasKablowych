#pragma once

#include "domain/types.h"

#include <QSqlDatabase>

namespace ktk {

class CatalogRepository final {
public:
    explicit CatalogRepository(QString databasePath = {});
    ~CatalogRepository();

    CatalogRepository(const CatalogRepository &) = delete;
    CatalogRepository &operator=(const CatalogRepository &) = delete;

    [[nodiscard]] bool open(QString *errorMessage = nullptr);
    [[nodiscard]] QVector<CatalogItem> allItems(QString *errorMessage = nullptr) const;
    [[nodiscard]] QString databasePath() const;

private:
    [[nodiscard]] bool createSchema(QString *errorMessage);
    [[nodiscard]] bool synchronizeSeed(QString *errorMessage);
    [[nodiscard]] bool importSeed(QString *errorMessage);

    QString m_connectionName;
    QString m_databasePath;
    QSqlDatabase m_database;
};

} // namespace ktk
