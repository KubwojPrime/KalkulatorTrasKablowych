#include "domain/calculator.h"
#include "domain/bakscatalog.h"
#include "domain/catalogfilter.h"
#include "domain/fireloadestimator.h"
#include "data/catalogrepository.h"
#include "io/xlsxprojectio.h"
#include "io/dxfexport.h"
#include "domain/cablelayout.h"

#include <xlsxdocument.h>

#include <QCoreApplication>
#include <QElapsedTimer>
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

void testDxf()
{
    ktk::ProjectData p;
    p.route.projectName = QStringLiteral("Próba żółć");
    p.route.internalWidthMm = 30;
    p.route.internalHeightMm = 20;
    ktk::CableRow small;
    small.designation = QStringLiteral("YKYżo 3 x 2,5");
    small.manufacturer = QStringLiteral("Test");
    small.quantity = 2;
    small.outerDiameterMm = 10;
    small.fireLoadMjPerM = 1.25;
    small.fireLoadEstimated = true;
    auto large = small;
    large.outerDiameterMm = 20;
    large.quantity = 1;
    p.cables = {small, large};
    const auto layout = ktk::cableLayout(p);
    require(layout.size() == 3, "CAD cable count");
    require(layout[0].row == 1 && layout[1].row == 0, "CAD descending diameter and row mapping");
    requireNear(layout[1].x, 20, "CAD second cable x");
    requireNear(layout[2].y, 20, "CAD next layer y");
    require(layout[2].overflow && !layout[1].overflow, "CAD overflow");
    QTemporaryDir dir;
    QString path = QString::fromLocal8Bit(qgetenv("KTK_TEST_DXF_OUTPUT"));
    if (path.isEmpty()) path = dir.filePath(QStringLiteral("test.dxf"));
    QString error;
    require(ktk::DxfExport::write(path, p, &error), "DXF export");
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "DXF read");
    const auto data = file.readAll();
    file.close();
    require(data.count("0\nCIRCLE\n") == 3, "DXF circles");
    require(data.contains(QStringLiteral("YKYżo").toUtf8()) && data.contains("1.250*"), "DXF unicode and estimate");
    require(data.contains("brak danych") && data.contains("$INSUNITS\n70\n4"), "DXF missing data and mm");
    p.cables[0].outerDiameterMm = 0;
    require(!ktk::DxfExport::write(path, p, &error), "DXF invalid data rejected");
    require(file.open(QIODevice::ReadOnly) && file.readAll() == data, "DXF failure preserves existing file");
    file.close();
    p.cables.clear();
    require(ktk::DxfExport::write(dir.filePath(QStringLiteral("empty.dxf")), p, &error), "DXF empty tray");
    small.quantity = 1001;
    p.cables = {small};
    require(ktk::cableLayout(p).size() == 1001, "CAD does not truncate quantities at 1000");
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

void testCalculatorBoundaries()
{
    ktk::ProjectData project;
    project.route.internalWidthMm = -100.0;
    project.route.internalHeightMm = 50.0;
    project.route.trayMassKgPerM = -1.0;
    project.route.coverMassKgPerM = -1.0;
    project.route.hangerBaseMassKg = -1.0;
    project.route.suspensionHeightM = -1.0;
    project.route.suspensionVerticalMassKgPerM = -1.0;
    project.route.supportSpacingM = 0.0;

    ktk::CableRow zeroQuantity;
    zeroQuantity.quantity = 0;
    zeroQuantity.outerDiameterMm = 10.0;

    ktk::CableRow negativeMass;
    negativeMass.quantity = 1;
    negativeMass.outerDiameterMm = 10.0;
    negativeMass.massKgPerKm = -1.0;

    ktk::CableRow validZeroValues;
    validZeroValues.quantity = 1;
    validZeroValues.outerDiameterMm = 5.0;
    validZeroValues.massKgPerKm = 0.0;
    validZeroValues.fireLoadMjPerM = 0.0;

    project.cables = {zeroQuantity, negativeMass, validZeroValues};
    const auto result = ktk::Calculator::calculate(project);

    requireNear(result.routeAreaMm2, 0.0, "negative route dimension");
    requireNear(result.fillPercent, 0.0, "zero route fill");
    requireNear(result.reservedCableAreaMm2, 25.0, "only valid cable area");
    requireNear(result.supportSystemMassKgPerM, 0.0, "negative support values clamped");
    require(result.invalidRows == 2, "invalid calculator row count");
    require(result.unknownMassRows == 0, "zero mass remains known");
    require(result.unknownFireLoadRows == 0, "zero fire load remains known");
}

void testBaksCatalog()
{
    QString error;
    const auto catalog = ktk::BaksCatalog::load(&error);
    require(error.isEmpty(), qPrintable(error));
    require(catalog.size() == 96, "BAKS catalog item count");
    int kcjCount = 0;
    QSet<int> kcjHeights;
    for (const auto &product : catalog) {
        if (product.symbol.contains(QStringLiteral("KCJ"))) {
            ++kcjCount;
            kcjHeights.insert(qRound(product.heightMm));
        }
    }
    require(kcjCount == 41, "BAKS KCJ item count");
    require(
        kcjHeights == QSet<int>({42, 50, 60, 80, 100, 110}),
        "BAKS KCJ height families");

    const auto route =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-kgr100h42-3"));
    const auto cover =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-pkl100-3"));
    const auto bracket =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-wws200"));
    const auto rod =
        ktk::BaksCatalog::findById(catalog, QStringLiteral("baks-pgm10-3"));
    const auto kcjH60 = ktk::BaksCatalog::findById(
        catalog,
        QStringLiteral("baks-kcj-kcoj200h60-3"));
    const auto kcjH110 = ktk::BaksCatalog::findById(
        catalog,
        QStringLiteral("baks-kcj600h110-3"));
    const auto kcjH80 = ktk::BaksCatalog::findById(
        catalog,
        QStringLiteral("baks-kcj500h80-3"));
    require(route.has_value(), "BAKS route");
    require(cover.has_value(), "BAKS cover");
    require(bracket.has_value(), "BAKS bracket");
    require(rod.has_value(), "BAKS rod");
    require(kcjH60.has_value(), "BAKS KCJ H60 route");
    require(kcjH80.has_value(), "BAKS KCJ H80 route");
    require(kcjH110.has_value(), "BAKS KCJ H110 route");
    requireNear(route->massKgPerUnit, 0.74, "BAKS KGR100 mass");
    require(route->sourcePage == 14, "BAKS KGR100 source page");
    requireNear(rod->massKgPerUnit / rod->lengthM, 0.5, "BAKS PGM10 kg/m");
    requireNear(kcjH60->massKgPerUnit, 2.13, "BAKS KCJ200H60 mass");
    require(kcjH60->catalogCode == QStringLiteral("161020"), "BAKS KCJ200H60 code");
    require(kcjH60->sourcePage == 59, "BAKS KCJ200H60 source page");
    requireNear(kcjH80->massKgPerUnit, 4.31, "BAKS KCJ500H80 mass");
    require(kcjH80->sourcePage == 105, "BAKS KCJ500H80 source page");
    requireNear(kcjH110->massKgPerUnit, 5.16, "BAKS KCJ600H110 mass");

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
    QElapsedTimer loadTimer;
    loadTimer.start();
    ktk::CatalogRepository repository(databasePath);
    QString error;
    require(repository.open(&error), qPrintable(error));
    const auto items = repository.allItems(&error);
    require(error.isEmpty(), qPrintable(error));
    require(items.size() >= 15000, "catalog item count");
    require(loadTimer.elapsed() < 60000, "catalog load performance under 60 seconds");

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

    QElapsedTimer filterTimer;
    filterTimer.start();
    int repeatedQueryMatches = 0;
    for (const auto &item : items) {
        if (realMatcher.matches(item)) {
            ++repeatedQueryMatches;
        }
    }
    require(repeatedQueryMatches == realYkyMatches, "repeatable catalog query");
    require(filterTimer.elapsed() < 10000, "catalog filter performance under 10 seconds");
}

struct TestWorkbookOptions {
    int schemaVersion = 4;
    QVariant width = 200.0;
    QVariant quantity = 2;
    QVariant diameter = 10.0;
    QVariant mass = 120.0;
    QVariant fireLoad = 0.75;
    bool includeProjectSheet = true;
    bool includeCableSheet = true;
};

void createTestProjectWorkbook(
    const QString &path,
    const TestWorkbookOptions &options = {})
{
    QXlsx::Document document;
    document.addSheet(QStringLiteral("Meta"));
    document.selectSheet(QStringLiteral("Meta"));
    document.write(1, 1, QStringLiteral("schema"));
    document.write(1, 2, QStringLiteral("KalkulatorTrasKablowych"));
    document.write(2, 1, QStringLiteral("schemaVersion"));
    document.write(2, 2, options.schemaVersion);

    if (options.includeProjectSheet) {
        document.addSheet(QStringLiteral("Projekt"));
        document.selectSheet(QStringLiteral("Projekt"));
        document.write(1, 2, QStringLiteral("Projekt zgodnosci"));
        document.write(2, 2, options.width);
        document.write(3, 2, 60.0);
        document.write(4, 2, 40.0);
        document.write(5, 2, 12.0);
        document.write(7, 2, 1.0);
        document.write(8, 2, 0.5);
        document.write(9, 2, 0.25);
        document.write(10, 2, 0.8);
        document.write(11, 2, 0.4);
        document.write(12, 2, 1.5);
    }

    if (options.includeCableSheet) {
        document.addSheet(QStringLiteral("Kable"));
        document.selectSheet(QStringLiteral("Kable"));
        document.write(2, 1, QStringLiteral("Producent testowy"));
        document.write(2, 2, QStringLiteral("YKY 3 x 2,5"));
        document.write(2, 3, QStringLiteral("TEST-1"));
        document.write(2, 4, options.quantity);
        document.write(2, 5, options.diameter);
        document.write(2, 6, options.mass);
        document.write(2, 7, options.fireLoad);
        if (options.schemaVersion >= 3) {
            document.write(2, 8, QStringLiteral("wartosc podana / producent"));
            document.write(2, 10, QStringLiteral("Dca-s2,d2"));
            document.write(2, 11, QStringLiteral("https://example.invalid/new"));
        } else {
            document.write(2, 8, QStringLiteral("Dca-s2,d2"));
            document.write(2, 9, QStringLiteral("https://example.invalid/legacy"));
        }
    }

    require(document.saveAs(path), "save generated XLSX fixture");
}

void testXlsxCompatibilityAndValidation(const QString &directoryPath)
{
    QString error;

    TestWorkbookOptions legacyOptions;
    legacyOptions.schemaVersion = 2;
    const QString legacyPath = directoryPath + QStringLiteral("/legacy-v2.xlsx");
    createTestProjectWorkbook(legacyPath, legacyOptions);
    ktk::ProjectData legacy;
    require(
        ktk::XlsxProjectIo::importProject(legacyPath, &legacy, &error),
        qPrintable(error));
    require(legacy.cables.size() == 1, "legacy XLSX cable count");
    requireNear(legacy.route.internalWidthMm, 200.0, "legacy XLSX width");
    require(
        legacy.cables.at(0).cprClass == QStringLiteral("Dca-s2,d2"),
        "legacy XLSX CPR column");
    require(
        legacy.cables.at(0).source == QStringLiteral("https://example.invalid/legacy"),
        "legacy XLSX source column");

    const auto expectRejected = [&](const QString &path, const char *label) {
        ktk::ProjectData unchanged;
        unchanged.route.projectName = QStringLiteral("nie zmieniaj");
        error.clear();
        require(!ktk::XlsxProjectIo::importProject(path, &unchanged, &error), label);
        require(!error.isEmpty(), "rejected XLSX explains error");
        require(
            unchanged.route.projectName == QStringLiteral("nie zmieniaj"),
            "rejected XLSX is atomic");
    };

    const QString unrelatedPath = directoryPath + QStringLiteral("/unrelated.xlsx");
    {
        QXlsx::Document unrelated;
        unrelated.write(1, 1, QStringLiteral("zwykly arkusz"));
        require(unrelated.saveAs(unrelatedPath), "save unrelated workbook");
    }
    expectRejected(unrelatedPath, "reject non-project XLSX");

    TestWorkbookOptions future;
    future.schemaVersion = 999;
    const QString futurePath = directoryPath + QStringLiteral("/future.xlsx");
    createTestProjectWorkbook(futurePath, future);
    expectRejected(futurePath, "reject future XLSX schema");

    TestWorkbookOptions missingCableSheet;
    missingCableSheet.includeCableSheet = false;
    const QString missingCablePath = directoryPath + QStringLiteral("/missing-cables.xlsx");
    createTestProjectWorkbook(missingCablePath, missingCableSheet);
    expectRejected(missingCablePath, "reject XLSX without cable sheet");

    TestWorkbookOptions invalidWidth;
    invalidWidth.width = QStringLiteral("dwieście");
    const QString invalidWidthPath = directoryPath + QStringLiteral("/invalid-width.xlsx");
    createTestProjectWorkbook(invalidWidthPath, invalidWidth);
    expectRejected(invalidWidthPath, "reject textual route width");

    TestWorkbookOptions missingWidth;
    missingWidth.width = QVariant();
    const QString missingWidthPath = directoryPath + QStringLiteral("/missing-width.xlsx");
    createTestProjectWorkbook(missingWidthPath, missingWidth);
    expectRejected(missingWidthPath, "reject missing route width");

    TestWorkbookOptions invalidQuantity;
    invalidQuantity.quantity = 0;
    const QString invalidQuantityPath = directoryPath + QStringLiteral("/invalid-quantity.xlsx");
    createTestProjectWorkbook(invalidQuantityPath, invalidQuantity);
    expectRejected(invalidQuantityPath, "reject zero cable quantity");

    TestWorkbookOptions invalidFireLoad;
    invalidFireLoad.fireLoad = -0.1;
    const QString invalidFireLoadPath = directoryPath + QStringLiteral("/invalid-fire-load.xlsx");
    createTestProjectWorkbook(invalidFireLoadPath, invalidFireLoad);
    expectRejected(invalidFireLoadPath, "reject negative fire load");
}

} // namespace

int main(int argc, char *argv[])
{
    std::cerr << "tests: start\n";
    QCoreApplication application(argc, argv);
    testCalculator();
    testDxf();
    std::cerr << "calculator: passed\n";
    testCalculatorBoundaries();
    std::cerr << "calculator boundaries: passed\n";
    testBaksCatalog();
    std::cerr << "BAKS catalog: passed\n";
    testCatalogFilter();
    std::cerr << "catalog filter: passed\n";
    testFireLoadEstimator();
    std::cerr << "fire load estimator: passed\n";
    QTemporaryDir directory;
    require(directory.isValid(), "temporary directory");
    std::cerr << "xlsx: temporary directory ready\n";
    testXlsxCompatibilityAndValidation(directory.path());
    std::cerr << "xlsx compatibility and validation: passed\n";
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
