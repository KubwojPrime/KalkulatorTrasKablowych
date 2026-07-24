#include "domain/calculator.h"
#include "data/catalogrepository.h"
#include "io/xlsxprojectio.h"

#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *label)
{
    if (!condition) {
        std::cerr << "failed: " << label << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void requireNear(double actual, double expected, const char *label)
{
    if (std::abs(actual - expected) > 1e-9) {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void testCalculator()
{
    ktk::ProjectData project;
    project.route.internalWidthMm = 100.0;
    project.route.internalHeightMm = 50.0;
    project.route.maximumFillPercent = 10.0;
    project.route.fireLoadLimitMjPerM = 5.0;
    project.route.trayMassKgPerM = 2.0;
    project.route.coverMassKgPerM = 0.5;
    project.route.hangerBaseMassKg = 1.2;
    project.route.suspensionHeightM = 0.5;
    project.route.suspensionVerticalMassKgPerM = 0.8;
    project.route.supportSpacingM = 2.0;

    ktk::CableRow first;
    first.quantity = 2;
    first.outerDiameterMm = 10.0;
    first.massKgPerKm = 100.0;
    first.fireLoadMjPerM = 1.5;

    ktk::CableRow second;
    second.quantity = 1;
    second.outerDiameterMm = 20.0;
    second.massKgPerKm = 500.0;
    second.fireLoadMjPerM = 2.5;

    ktk::CableRow unknown;
    unknown.quantity = 1;
    unknown.outerDiameterMm = 5.0;
    unknown.massKgPerKm.reset();

    project.cables = {first, second, unknown};
    const auto result = ktk::Calculator::calculate(project);

    requireNear(result.reservedCableAreaMm2, 625.0, "reserved area");
    requireNear(result.routeAreaMm2, 5000.0, "route area");
    requireNear(result.fillPercent, 12.5, "fill");
    requireNear(result.cableMassKgPerM, 0.7, "known cable mass");
    requireNear(result.supportSystemMassKgPerM, 3.3, "support mass");
    requireNear(result.totalInstalledMassKgPerM, 4.0, "known total mass");
    requireNear(result.knownFireLoadMjPerM, 5.5, "fire load");
    require(result.unknownMassRows == 1, "unknown mass count");
    require(result.unknownFireLoadRows == 1, "unknown fire load count");
    require(result.exceedsFillLimit, "fill limit");
    require(result.exceedsFireLoadLimit, "fire load limit");
}

void createLegacyCatalog(const QString &databasePath)
{
    const QString connection = QStringLiteral("legacy-test-setup");
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(databasePath);
        require(database.open(), "open legacy SQLite");
        QSqlQuery query(database);
        require(query.exec(QStringLiteral(
                    "CREATE TABLE cable_catalog ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "manufacturer TEXT NOT NULL,"
                    "designation TEXT NOT NULL,"
                    "catalog_code TEXT NOT NULL DEFAULT '',"
                    "diameter_mm REAL NOT NULL,"
                    "mass_kg_km REAL NOT NULL,"
                    "fire_load_mj_m REAL NULL,"
                    "cpr_class TEXT NOT NULL DEFAULT '',"
                    "source TEXT NOT NULL DEFAULT '',"
                    "source_date TEXT NOT NULL DEFAULT '',"
                    "notes TEXT NOT NULL DEFAULT '',"
                    "verified INTEGER NOT NULL DEFAULT 0,"
                    "UNIQUE(manufacturer, designation, catalog_code))")),
                "create legacy schema");
        require(query.exec(QStringLiteral(
                    "INSERT INTO cable_catalog("
                    "manufacturer, designation, diameter_mm, mass_kg_km"
                    ") VALUES('Własny', 'Zachowany wpis', 9.5, 123.0)")),
                "insert legacy item");
        database.close();
    }
    QSqlDatabase::removeDatabase(connection);
}

void testCatalog(const QString &databasePath)
{
    createLegacyCatalog(databasePath);
    ktk::CatalogRepository repository(databasePath);
    QString error;
    require(repository.open(&error), qPrintable(error));
    const auto items = repository.allItems(&error);
    require(error.isEmpty(), qPrintable(error));
    require(items.size() >= 15000, "catalog item count");

    QSet<QString> manufacturers;
    bool cobiMissingMass = false;
    bool tfkDirectFireLoad = false;
    bool legacyItemPreserved = false;
    for (const auto &item : items) {
        manufacturers.insert(item.cable.manufacturer);
        if (item.cable.manufacturer == QStringLiteral("CobiCabling")
            && !item.cable.massKgPerKm.has_value()) {
            cobiMissingMass = true;
        }
        if (item.cable.manufacturer == QStringLiteral("TELE-FONIKA Kable")
            && item.cable.fireLoadMjPerM.value_or(0.0) > 0.0) {
            tfkDirectFireLoad = true;
        }
        if (item.cable.manufacturer == QStringLiteral("Własny")
            && item.cable.designation == QStringLiteral("Zachowany wpis")
            && item.cable.massKgPerKm.value_or(0.0) == 123.0) {
            legacyItemPreserved = true;
        }
    }
    require(manufacturers.contains(QStringLiteral("TELE-FONIKA Kable")), "TFK catalog");
    require(manufacturers.contains(QStringLiteral("ELPAR")), "ELPAR catalog");
    require(manufacturers.contains(QStringLiteral("BITNER")), "BITNER catalog");
    require(manufacturers.contains(QStringLiteral("CobiCabling")), "Cobi catalog");
    require(cobiMissingMass, "Cobi missing mass remains unknown");
    require(tfkDirectFireLoad, "TFK direct heat of combustion");
    require(legacyItemPreserved, "legacy custom catalog item preserved");
}

} // namespace

int main(int argc, char *argv[])
{
    std::cerr << "tests: start\n";
    QCoreApplication application(argc, argv);
    testCalculator();
    std::cerr << "calculator: passed\n";
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    std::cerr << "xlsx: temporary directory ready\n";
    testCatalog(directory.filePath(QStringLiteral("catalog.sqlite")));
    std::cerr << "catalog: passed\n";

    ktk::ProjectData original;
    original.route.projectName = QStringLiteral("Test ąęł");
    original.route.internalWidthMm = 200.0;
    original.route.internalHeightMm = 80.0;
    original.route.maximumFillPercent = 45.0;
    original.route.fireLoadLimitMjPerM = 12.5;
    original.route.trayMassKgPerM = 2.3;
    original.route.coverMassKgPerM = 0.7;
    original.route.hangerBaseMassKg = 1.1;
    original.route.suspensionHeightM = 0.8;
    original.route.suspensionVerticalMassKgPerM = 0.6;
    original.route.supportSpacingM = 1.25;

    ktk::CableRow cable;
    cable.manufacturer = QStringLiteral("Producent");
    cable.designation = QStringLiteral("Kabel 5 × 10");
    cable.catalogCode = QStringLiteral("ABC-123");
    cable.quantity = 7;
    cable.outerDiameterMm = 12.4;
    cable.massKgPerKm = 450.0;
    cable.fireLoadMjPerM = 0.82;
    cable.cprClass = QStringLiteral("B2ca-s1a,d1,a1");
    cable.source = QStringLiteral("https://example.invalid/datasheet");
    original.cables.append(cable);

    ktk::CableRow unknown = cable;
    unknown.designation = QStringLiteral("Bez MJ/m");
    unknown.quantity = 2;
    unknown.massKgPerKm.reset();
    unknown.fireLoadMjPerM.reset();
    original.cables.append(unknown);

    const auto result = ktk::Calculator::calculate(original);
    const QString path = directory.filePath(QStringLiteral("projekt.xlsx"));
    QString error;
    const bool exported = ktk::XlsxProjectIo::exportProject(path, original, result, &error);
    if (!exported) {
        std::cerr << "export failed: " << qPrintable(error) << '\n';
        return EXIT_FAILURE;
    }
    require(QFile::exists(path), "xlsx exists");
    std::cerr << "xlsx: export ready\n";

    ktk::ProjectData loaded;
    const bool imported = ktk::XlsxProjectIo::importProject(path, &loaded, &error);
    if (!imported) {
        std::cerr << "import failed: " << qPrintable(error) << '\n';
        return EXIT_FAILURE;
    }
    std::cerr << "xlsx: import ready\n";

    require(loaded.route.projectName == original.route.projectName, "project name");
    requireNear(loaded.route.internalWidthMm, 200.0, "width");
    requireNear(loaded.route.suspensionHeightM, 0.8, "suspension height");
    require(loaded.cables.size() == 2, "cable count");
    require(loaded.cables.at(0).manufacturer == cable.manufacturer, "manufacturer");
    require(loaded.cables.at(0).designation == cable.designation, "designation");
    require(loaded.cables.at(0).quantity == 7, "quantity");
    requireNear(loaded.cables.at(0).outerDiameterMm, 12.4, "diameter");
    requireNear(loaded.cables.at(0).massKgPerKm.value_or(-1.0), 450.0, "mass");
    requireNear(loaded.cables.at(0).fireLoadMjPerM.value_or(-1.0), 0.82, "fire load");
    require(!loaded.cables.at(1).massKgPerKm.has_value(), "missing mass");
    require(!loaded.cables.at(1).fireLoadMjPerM.has_value(), "missing fire load");

    std::cout << "xlsx roundtrip passed\n";
    return EXIT_SUCCESS;
}
