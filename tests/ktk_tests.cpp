#include "domain/calculator.h"
#include "domain/bakscatalog.h"
#include "domain/catalogfilter.h"
#include "domain/fireloadestimator.h"
#include "data/catalogrepository.h"
#include "io/xlsxprojectio.h"

#include <xlsxdocument.h>

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
    second.fireLoadEstimated = true;
    second.fireLoadBasis = QStringLiteral("test estimate");

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
    requireNear(result.confirmedFireLoadMjPerM, 3.0, "confirmed fire load");
    requireNear(result.estimatedFireLoadMjPerM, 2.5, "estimated fire load");
    require(result.unknownMassRows == 1, "unknown mass count");
    require(result.unknownFireLoadRows == 1, "unknown fire load count");
    require(result.estimatedFireLoadRows == 1, "estimated fire load rows");
    require(result.exceedsFillLimit, "fill limit");
    require(result.exceedsFireLoadLimit, "fire load limit");
}

void testBaksCatalog()
{
    QString error;
    const auto catalog = ktk::BaksCatalog::load(&error);
    require(error.isEmpty(), qPrintable(error));
    require(catalog.size() == 55, "BAKS catalog item count");

    const auto route =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-kgr100h42-3"));
    const auto cover =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-pkl100-3"));
    const auto bracket =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-wws200"));
    const auto rod =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-pgm10-3"));
    require(route.has_value(), "BAKS route");
    require(cover.has_value(), "BAKS cover");
    require(bracket.has_value(), "BAKS bracket");
    require(rod.has_value(), "BAKS rod");
    requireNear(route->massKgPerUnit, 0.74, "BAKS KGR100 mass");
    require(route->sourcePage == 14, "BAKS KGR100 source page");
    requireNear(rod->massKgPerUnit / rod->lengthM, 0.5, "BAKS PGM10 kg/m");

    const QVector<ktk::RouteAssemblyItem> assembly = {
        {*route, 1.0},
        {*cover, 1.0},
        {*bracket, 2.0},
        {*rod, 2.0}
    };
    const auto summary = ktk::BaksCatalog::summarize(assembly);
    requireNear(summary.trayMassKgPerM, 0.74, "BAKS tray summary");
    requireNear(summary.coverMassKgPerM, 0.72, "BAKS cover summary");
    requireNear(summary.fixedMassKgPerSupport, 0.76, "BAKS fixed summary");
    requireNear(
        summary.verticalMassKgPerSuspensionM,
        1.0,
        "BAKS vertical summary");
}

void testFireLoadEstimator()
{
    ktk::CableRow yky;
    yky.designation = QStringLiteral("YKYżo 0,6/1 kV 3 x 2,5 mm²");
    yky.outerDiameterMm = 11.0;
    yky.massKgPerKm = 200.0;

    const auto estimate = ktk::FireLoadEstimator::estimate(yky);
    require(estimate.has_value(), "YKY fire load estimate");
    requireNear(estimate->fireLoadMjPerM, 3.32, "YKY conservative estimate");
    require(
        estimate->material.contains(QStringLiteral("PVC")),
        "YKY estimated material");
    require(
        estimate->basis.contains(QStringLiteral("Oszacowanie materiałowe*")),
        "estimate basis");

    require(
        ktk::FireLoadEstimator::applyIfMissing(&yky),
        "apply fire load estimate");
    require(yky.fireLoadEstimated, "estimated marker");
    require(yky.fireLoadMjPerM.has_value(), "estimated value assigned");

    yky.fireLoadMjPerM = 0.75;
    yky.fireLoadEstimated = false;
    yky.fireLoadBasis.clear();
    require(
        !ktk::FireLoadEstimator::applyIfMissing(&yky),
        "manufacturer value is not overwritten");
    requireNear(yky.fireLoadMjPerM.value(), 0.75, "manufacturer fire load");

    ktk::CableRow unsupported;
    unsupported.designation = QStringLiteral("Kabel specjalny 3 x 2,5 mm²");
    unsupported.outerDiameterMm = 10.0;
    unsupported.massKgPerKm = 150.0;
    require(
        !ktk::FireLoadEstimator::estimate(unsupported).has_value(),
        "unknown material remains unknown");
}

void testCatalogFilter()
{
    ktk::CatalogItem yky;
    yky.cable.manufacturer = QStringLiteral("TELE-FONIKA Kable");
    yky.cable.designation =
        QStringLiteral("YKYżo 0,6/1 kV 3 x 2,5 mm²");
    yky.cable.catalogCode = QStringLiteral("TFK-ABC-123");
    yky.cable.cprClass = QStringLiteral("Dca-s2,d2");

    require(
        ktk::CatalogFilter::tokenizedMatch(
            QStringLiteral("YKY 3 x 2,5"),
            yky.cable.designation),
        "outlook-style tokens");
    require(
        ktk::CatalogFilter::tokenizedMatch(
            QStringLiteral("2.5 yky 3"),
            yky.cable.designation),
        "token order and decimal separator");
    require(
        ktk::CatalogFilter::tokenizedMatch(
            QStringLiteral("YKYzo 3x2.50"),
            yky.cable.designation),
        "compact conductor configuration");
    require(
        !ktk::CatalogFilter::tokenizedMatch(
            QStringLiteral("YKY 4 x 2,5"),
            yky.cable.designation),
        "all tokens required");
    require(
        !ktk::CatalogFilter::tokenizedMatch(
            QStringLiteral("YKY 30"),
            yky.cable.designation),
        "numeric tokens are exact");
    require(
        ktk::CatalogFilter::cableFamily(yky)
            == QStringLiteral("YKYżo 0,6/1 kV"),
        "cable family");
    require(
        ktk::CatalogFilter::insulationTags(yky).contains(
            QStringLiteral("PVC")),
        "PVC classification");

    ktk::CatalogFilterCriteria criteria;
    criteria.query = QStringLiteral("YKY 3 2,5");
    criteria.manufacturer = QStringLiteral("TELE-FONIKA Kable");
    criteria.cableType = QStringLiteral("YKY 0,6");
    criteria.insulation = QStringLiteral("PVC");
    criteria.cprClass = QStringLiteral("Dca-s2,d2");
    require(
        ktk::CatalogFilter::matches(yky, criteria),
        "combined catalog filters");
    criteria.manufacturer = QStringLiteral("BITNER");
    require(
        !ktk::CatalogFilter::matches(yky, criteria),
        "manufacturer filter");

    ktk::CatalogItem fireResistant;
    fireResistant.cable.designation =
        QStringLiteral("N2XH-J FE180/PH90/E90 5G10 mm²");
    const auto materialTags =
        ktk::CatalogFilter::insulationTags(fireResistant);
    require(
        materialTags.contains(
            QStringLiteral("XLPE / polietylen sieciowany")),
        "XLPE classification");
    require(
        materialTags.contains(QStringLiteral("bezhalogenowa (LSZH)")),
        "LSZH classification");
    const auto fireTags =
        ktk::CatalogFilter::fireResistanceTags(fireResistant);
    require(fireTags.contains(QStringLiteral("FE180")), "FE180 filter");
    require(fireTags.contains(QStringLiteral("PH90")), "PH90 filter");
    require(fireTags.contains(QStringLiteral("E90")), "E90 filter");

    ktk::CatalogItem falseE30;
    falseE30.cable.designation =
        QStringLiteral("INFORMACJE TECHNICZNE 30 x 2,5 mm²");
    require(
        !ktk::CatalogFilter::fireResistanceTags(falseE30).contains(
            QStringLiteral("E30")),
        "avoid false E30 from technical text");
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
    int estimatedFireLoads = 0;
    int directEstimateBenchmarks = 0;
    int realYkyMatches = 0;
    ktk::CatalogFilterCriteria realQuery;
    realQuery.query = QStringLiteral("YKY 3 x 2,5");
    const ktk::CatalogFilter realMatcher(realQuery);
    for (const auto &item : items) {
        manufacturers.insert(item.cable.manufacturer);
        if (realMatcher.matches(item)) {
            ++realYkyMatches;
        }
        if (item.cable.manufacturer == QStringLiteral("CobiCabling")
            && !item.cable.massKgPerKm.has_value()) {
            cobiMissingMass = true;
        }
        if (item.cable.fireLoadEstimated) {
            ++estimatedFireLoads;
        }
        if (item.cable.manufacturer == QStringLiteral("TELE-FONIKA Kable")
            && item.cable.fireLoadMjPerM.value_or(0.0) > 0.0
            && !item.cable.fireLoadEstimated) {
            tfkDirectFireLoad = true;
            const auto benchmark =
                ktk::FireLoadEstimator::estimate(item.cable);
            if (benchmark.has_value()) {
                ++directEstimateBenchmarks;
                require(
                    benchmark->fireLoadMjPerM + 1e-9
                        >= item.cable.fireLoadMjPerM.value(),
                    "estimate is conservative against TFK value");
            }
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
    require(estimatedFireLoads > 10000, "material fire load estimates");
    require(
        directEstimateBenchmarks >= 100,
        "TFK direct-value estimate benchmarks");
    require(legacyItemPreserved, "legacy custom catalog item preserved");
    require(realYkyMatches > 0, "real catalog Outlook-style YKY query");
}

} // namespace

int main(int argc, char *argv[])
{
    std::cerr << "tests: start\n";
    QCoreApplication application(argc, argv);
    testCalculator();
    std::cerr << "calculator: passed\n";
    testBaksCatalog();
    std::cerr << "BAKS catalog: passed\n";
    testCatalogFilter();
    std::cerr << "catalog filter: passed\n";
    testFireLoadEstimator();
    std::cerr << "fire load estimator: passed\n";
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

    ktk::BaksProduct trayProduct;
    trayProduct.id = QStringLiteral("baks-test-tray");
    trayProduct.role = QStringLiteral("route");
    trayProduct.name = QStringLiteral("Korytko testowe");
    trayProduct.symbol = QStringLiteral("KGR200H42/3");
    trayProduct.catalogCode = QStringLiteral("141716");
    trayProduct.widthMm = 200.0;
    trayProduct.heightMm = 42.0;
    trayProduct.lengthM = 3.0;
    trayProduct.massKgPerUnit = 1.10;
    trayProduct.massUnit = QStringLiteral("kg/m");
    trayProduct.sourceFile =
        QStringLiteral("source-materials/baks/BAKS_Korytka_kablowe_2024.pdf");
    trayProduct.sourcePage = 14;
    trayProduct.sourceUrl =
        QStringLiteral("https://katalog.baks.com.pl/test");
    trayProduct.sourceDate = QStringLiteral("2026-07-24");
    original.routeAssembly.append({trayProduct, 1.0});

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

    ktk::CableRow estimated = cable;
    estimated.designation = QStringLiteral("YKYżo 3 x 2,5 mm²");
    estimated.quantity = 1;
    estimated.fireLoadMjPerM = 0.95;
    estimated.fireLoadEstimated = true;
    estimated.fireLoadBasis =
        QStringLiteral("Oszacowanie materiałowe*: test");
    original.cables.append(estimated);

    const auto result = ktk::Calculator::calculate(original);
    const QString requestedOutput =
        QString::fromLocal8Bit(qgetenv("KTK_TEST_XLSX_OUTPUT")).trimmed();
    const QString path =
        requestedOutput.isEmpty()
            ? directory.filePath(QStringLiteral("projekt.xlsx"))
            : requestedOutput;
    QString error;
    const bool exported = ktk::XlsxProjectIo::exportProject(path, original, result, &error);
    if (!exported) {
        std::cerr << "export failed: " << qPrintable(error) << '\n';
        return EXIT_FAILURE;
    }
    require(QFile::exists(path), "xlsx exists");
    std::cerr << "xlsx: export ready\n";
    QXlsx::Document exportedWorkbook(path);
    require(exportedWorkbook.load(), "load exported workbook");
    require(
        exportedWorkbook.selectSheet(QStringLiteral("Kable")),
        "select exported cable sheet");
    require(
        exportedWorkbook.read(4, 7).toString().endsWith(QLatin1Char('*')),
        "estimated XLSX value has star");
    require(
        exportedWorkbook.read(4, 8).toString().contains(
            QStringLiteral("oszacowanie")),
        "estimated XLSX origin");
    require(
        exportedWorkbook.selectSheet(QStringLiteral("Zestaw BAKS")),
        "select exported BAKS sheet");
    require(
        exportedWorkbook.read(2, 4).toString()
            == QStringLiteral("KGR200H42/3"),
        "BAKS symbol exported");
    requireNear(
        exportedWorkbook.read(2, 7).toDouble(),
        1.10,
        "BAKS mass exported");

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
    require(loaded.routeAssembly.size() == 1, "BAKS assembly count");
    require(
        loaded.routeAssembly.at(0).product.catalogCode
            == trayProduct.catalogCode,
        "BAKS catalog code roundtrip");
    require(
        loaded.routeAssembly.at(0).product.sourcePage == 14,
        "BAKS source page roundtrip");
    require(loaded.cables.size() == 3, "cable count");
    require(loaded.cables.at(0).manufacturer == cable.manufacturer, "manufacturer");
    require(loaded.cables.at(0).designation == cable.designation, "designation");
    require(loaded.cables.at(0).quantity == 7, "quantity");
    requireNear(loaded.cables.at(0).outerDiameterMm, 12.4, "diameter");
    requireNear(loaded.cables.at(0).massKgPerKm.value_or(-1.0), 450.0, "mass");
    requireNear(loaded.cables.at(0).fireLoadMjPerM.value_or(-1.0), 0.82, "fire load");
    require(!loaded.cables.at(1).massKgPerKm.has_value(), "missing mass");
    require(!loaded.cables.at(1).fireLoadMjPerM.has_value(), "missing fire load");
    require(
        loaded.cables.at(2).fireLoadEstimated,
        "estimated fire load marker roundtrip");
    requireNear(
        loaded.cables.at(2).fireLoadMjPerM.value_or(-1.0),
        0.95,
        "estimated fire load roundtrip");
    require(
        loaded.cables.at(2).fireLoadBasis == estimated.fireLoadBasis,
        "estimate basis roundtrip");

    std::cout << "xlsx roundtrip passed\n";
    return EXIT_SUCCESS;
}
