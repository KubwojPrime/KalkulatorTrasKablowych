#include "data/catalogrepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace ktk {

namespace {

void setError(QString *target, const QString &message)
{
    if (target) {
        *target = message;
    }
}

} // namespace

CatalogRepository::CatalogRepository(QString databasePath)
    : m_connectionName(QStringLiteral("catalog-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    const QString dataDirectory = databasePath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        : QFileInfo(databasePath).absolutePath();
    QDir().mkpath(dataDirectory);
    m_databasePath = databasePath.isEmpty()
        ? QDir(dataDirectory).filePath(QStringLiteral("catalog.sqlite"))
        : QFileInfo(databasePath).absoluteFilePath();
}

CatalogRepository::~CatalogRepository()
{
    if (m_database.isValid()) {
        m_database.close();
    }
    m_database = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool CatalogRepository::open(QString *errorMessage)
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        setError(errorMessage, QStringLiteral("Brak sterownika Qt SQLite."));
        return false;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_databasePath);
    if (!m_database.open()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    return createSchema(errorMessage) && synchronizeSeed(errorMessage);
}

QVector<CatalogItem> CatalogRepository::allItems(QString *errorMessage) const
{
    QVector<CatalogItem> items;
    if (!m_database.isOpen()) {
        setError(errorMessage, QStringLiteral("Baza katalogowa nie jest otwarta."));
        return items;
    }

    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT id, manufacturer, designation, catalog_code, diameter_mm, mass_kg_km, "
            "mass_known, fire_load_mj_m, cpr_class, source, source_date, notes, verified, "
            "source_file, source_page, source_url, extraction_method "
            "FROM cable_catalog ORDER BY manufacturer, designation"))) {
        setError(errorMessage, query.lastError().text());
        return items;
    }

    while (query.next()) {
        CatalogItem item;
        item.id = query.value(0).toLongLong();
        item.cable.manufacturer = query.value(1).toString();
        item.cable.designation = query.value(2).toString();
        item.cable.catalogCode = query.value(3).toString();
        item.cable.outerDiameterMm = query.value(4).toDouble();
        if (query.value(6).toBool()) {
            item.cable.massKgPerKm = query.value(5).toDouble();
        }
        if (!query.value(7).isNull()) {
            item.cable.fireLoadMjPerM = query.value(7).toDouble();
        }
        item.cable.cprClass = query.value(8).toString();
        item.cable.source = query.value(9).toString();
        item.sourceDate = query.value(10).toString();
        item.notes = query.value(11).toString();
        item.verified = query.value(12).toBool();
        item.sourceFile = query.value(13).toString();
        item.sourcePage = query.value(14).toInt();
        item.sourceUrl = query.value(15).toString();
        item.extractionMethod = query.value(16).toString();
        items.append(item);
    }

    return items;
}

QString CatalogRepository::databasePath() const
{
    return m_databasePath;
}

bool CatalogRepository::createSchema(QString *errorMessage)
{
    QSqlQuery query(m_database);
    const QString statement = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS cable_catalog ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "manufacturer TEXT NOT NULL,"
        "designation TEXT NOT NULL,"
        "catalog_code TEXT NOT NULL DEFAULT '',"
        "diameter_mm REAL NOT NULL,"
        "mass_kg_km REAL NOT NULL DEFAULT 0,"
        "mass_known INTEGER NOT NULL DEFAULT 0,"
        "fire_load_mj_m REAL NULL,"
        "cpr_class TEXT NOT NULL DEFAULT '',"
        "source TEXT NOT NULL DEFAULT '',"
        "source_file TEXT NOT NULL DEFAULT '',"
        "source_page INTEGER NOT NULL DEFAULT 0,"
        "source_url TEXT NOT NULL DEFAULT '',"
        "extraction_method TEXT NOT NULL DEFAULT '',"
        "source_date TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "verified INTEGER NOT NULL DEFAULT 0,"
        "seed_key TEXT NOT NULL DEFAULT '',"
        "UNIQUE(manufacturer, designation, catalog_code)"
        ")");

    if (!query.exec(statement)) {
        setError(errorMessage, query.lastError().text());
        return false;
    }

    const auto hasColumn = [this](const QString &name) {
        QSqlQuery columns(m_database);
        if (!columns.exec(QStringLiteral("PRAGMA table_info(cable_catalog)"))) {
            return false;
        }
        while (columns.next()) {
            if (columns.value(1).toString() == name) {
                return true;
            }
        }
        return false;
    };
    const auto ensureColumn =
        [this, &hasColumn, errorMessage](const QString &name, const QString &definition) {
            if (hasColumn(name)) {
                return true;
            }
            QSqlQuery alter(m_database);
            if (!alter.exec(
                    QStringLiteral("ALTER TABLE cable_catalog ADD COLUMN ") + definition)) {
                setError(errorMessage, alter.lastError().text());
                return false;
            }
            return true;
        };
    if (!ensureColumn(
            QStringLiteral("mass_known"),
            QStringLiteral("mass_known INTEGER NOT NULL DEFAULT 1"))
        || !ensureColumn(
            QStringLiteral("source_file"),
            QStringLiteral("source_file TEXT NOT NULL DEFAULT ''"))
        || !ensureColumn(
            QStringLiteral("source_page"),
            QStringLiteral("source_page INTEGER NOT NULL DEFAULT 0"))
        || !ensureColumn(
            QStringLiteral("source_url"),
            QStringLiteral("source_url TEXT NOT NULL DEFAULT ''"))
        || !ensureColumn(
            QStringLiteral("extraction_method"),
            QStringLiteral("extraction_method TEXT NOT NULL DEFAULT ''"))
        || !ensureColumn(
            QStringLiteral("seed_key"),
            QStringLiteral("seed_key TEXT NOT NULL DEFAULT ''"))) {
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS catalog_metadata ("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL"
            ")"))
        || !query.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_cable_catalog_seed_key "
            "ON cable_catalog(seed_key)"))) {
        setError(errorMessage, query.lastError().text());
        return false;
    }
    return true;
}

bool CatalogRepository::synchronizeSeed(QString *errorMessage)
{
    return importSeed(errorMessage);
}

bool CatalogRepository::importSeed(QString *errorMessage)
{
    QFile file(QStringLiteral(":/catalog-seed.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Nie można otworzyć startowego katalogu kabli."));
        return false;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("Błąd katalogu startowego: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject root = document.object();
    const QString datasetVersion =
        root.value(QStringLiteral("datasetVersion")).toString().trimmed();
    if (datasetVersion.isEmpty()) {
        setError(errorMessage, QStringLiteral("Katalog startowy nie ma wersji zestawu danych."));
        return false;
    }

    QSqlQuery versionQuery(m_database);
    versionQuery.prepare(QStringLiteral(
        "SELECT value FROM catalog_metadata WHERE key = 'datasetVersion'"));
    if (!versionQuery.exec()) {
        setError(errorMessage, versionQuery.lastError().text());
        return false;
    }
    if (versionQuery.next() && versionQuery.value(0).toString() == datasetVersion) {
        return true;
    }

    if (!m_database.transaction()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }

    QSqlQuery removeOldSeed(m_database);
    if (!removeOldSeed.exec(QStringLiteral(
            "DELETE FROM cable_catalog WHERE seed_key LIKE 'official:%'"))) {
        m_database.rollback();
        setError(errorMessage, removeOldSeed.lastError().text());
        return false;
    }

    QSqlQuery insert(m_database);
    insert.prepare(QStringLiteral(
        "INSERT INTO cable_catalog("
        "seed_key, manufacturer, designation, catalog_code, diameter_mm, mass_kg_km, "
        "mass_known, fire_load_mj_m, cpr_class, source, source_file, source_page, "
        "source_url, extraction_method, source_date, notes, verified"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(manufacturer, designation, catalog_code) DO UPDATE SET "
        "seed_key=excluded.seed_key, diameter_mm=excluded.diameter_mm, "
        "mass_kg_km=excluded.mass_kg_km, mass_known=excluded.mass_known, "
        "fire_load_mj_m=excluded.fire_load_mj_m, cpr_class=excluded.cpr_class, "
        "source=excluded.source, source_file=excluded.source_file, "
        "source_page=excluded.source_page, source_url=excluded.source_url, "
        "extraction_method=excluded.extraction_method, "
        "source_date=excluded.source_date, notes=excluded.notes, "
        "verified=excluded.verified"));

    const auto items = root.value(QStringLiteral("items")).toArray();
    for (const auto &value : items) {
        const auto object = value.toObject();
        const auto mass = object.value(QStringLiteral("massKgPerKm"));
        const bool massKnown = mass.isDouble();
        insert.bindValue(0, object.value(QStringLiteral("seedKey")).toString());
        insert.bindValue(1, object.value(QStringLiteral("manufacturer")).toString());
        insert.bindValue(2, object.value(QStringLiteral("designation")).toString());
        insert.bindValue(3, object.value(QStringLiteral("catalogCode")).toString());
        insert.bindValue(4, object.value(QStringLiteral("outerDiameterMm")).toDouble());
        insert.bindValue(5, massKnown ? mass.toDouble() : 0.0);
        insert.bindValue(6, massKnown);
        const auto fireLoad = object.value(QStringLiteral("fireLoadMjPerM"));
        insert.bindValue(7, fireLoad.isNull() || fireLoad.isUndefined()
                                ? QVariant()
                                : QVariant(fireLoad.toDouble()));
        insert.bindValue(8, object.value(QStringLiteral("cprClass")).toString());
        insert.bindValue(9, object.value(QStringLiteral("source")).toString());
        insert.bindValue(10, object.value(QStringLiteral("sourceFile")).toString());
        insert.bindValue(11, object.value(QStringLiteral("sourcePage")).toInt());
        insert.bindValue(12, object.value(QStringLiteral("sourceUrl")).toString());
        insert.bindValue(13, object.value(QStringLiteral("extractionMethod")).toString());
        insert.bindValue(14, object.value(QStringLiteral("sourceDate")).toString());
        insert.bindValue(15, object.value(QStringLiteral("notes")).toString());
        insert.bindValue(16, object.value(QStringLiteral("verified")).toBool());

        if (!insert.exec()) {
            m_database.rollback();
            setError(errorMessage, insert.lastError().text());
            return false;
        }
        insert.finish();
    }

    QSqlQuery saveVersion(m_database);
    saveVersion.prepare(QStringLiteral(
        "INSERT INTO catalog_metadata(key, value) VALUES('datasetVersion', ?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    saveVersion.bindValue(0, datasetVersion);
    if (!saveVersion.exec()) {
        m_database.rollback();
        setError(errorMessage, saveVersion.lastError().text());
        return false;
    }

    if (!m_database.commit()) {
        setError(errorMessage, m_database.lastError().text());
        return false;
    }
    return true;
}

} // namespace ktk
