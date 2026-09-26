#include "ui/mainwindow.h"

#include "domain/bakscatalog.h"
#include "domain/calculator.h"
#include "io/xlsxprojectio.h"
#include "io/dxfexport.h"
#include "io/projectrecovery.h"
#include <QTimer>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include "ui/baksassemblydialog.h"
#include "ui/cabletablemodel.h"
#include "ui/catalogdialog.h"
#include "ui/routevisualizationwidget.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

namespace ktk {

MainWindow::MainWindow(QWidget *parent, const QString &sessionDirectory)
    : QMainWindow(parent), m_sessionDirectory(sessionDirectory)
{
    resize(1280, 780);
    setMinimumSize(980, 640);
    setWindowTitle(tr("Kalkulator Tras Kablowych[*]"));

    m_cableModel = new CableTableModel(this);
    buildUi();
    loadCatalog();
    loadBaksCatalog();
    connectInputSignals();
    recalculate();
    m_ready = true;
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setSingleShot(true);
    connect(m_autosaveTimer, &QTimer::timeout, this, &MainWindow::autosave);
    auto *periodic = new QTimer(this);
    periodic->setInterval(30000);
    connect(periodic, &QTimer::timeout, this, &MainWindow::autosave);
    periodic->start();
    QTimer::singleShot(0, this, &MainWindow::recoverProject);
}

void MainWindow::recalculate()
{
    if (m_ready && !m_loading) {
        m_dirty = true;
        m_autosavePending = true;
        setWindowModified(true);
        if (m_autosaveTimer) m_autosaveTimer->start(5000);
    }
    const ProjectData project = currentProject();
    const CalculationResult result = Calculator::calculate(project);

    setResultLabel(m_reservedAreaResult, result.reservedCableAreaMm2, 1, tr("mm²"));
    setResultLabel(m_routeAreaResult, result.routeAreaMm2, 1, tr("mm²"));
    setResultLabel(m_fillResult, result.fillPercent, 2, tr("%"));
    setResultLabel(m_cableMassResult, result.cableMassKgPerM, 3, tr("kg/m"));
    setResultLabel(m_supportMassResult, result.supportSystemMassKgPerM, 3, tr("kg/m"));
    setResultLabel(m_totalMassResult, result.totalInstalledMassKgPerM, 3, tr("kg/m"));
    setResultLabel(m_fireLoadResult, result.knownFireLoadMjPerM, 3, tr("MJ/m"));
    if (result.estimatedFireLoadRows > 0) {
        m_fireLoadResult->setText(m_fireLoadResult->text() + QStringLiteral("*"));
    }
    if (result.unknownMassRows > 0) {
        m_cableMassResult->setText(
            QStringLiteral("≥ %1").arg(m_cableMassResult->text()));
        m_totalMassResult->setText(
            QStringLiteral("≥ %1").arg(m_totalMassResult->text()));
    }

    QStringList notices;
    bool critical = false;
    bool caution = false;
    if (result.invalidRows > 0) {
        notices << tr("%n nieprawidłowy wiersz został pominięty.",
                      nullptr, result.invalidRows);
        critical = true;
    }
    if (result.exceedsFillLimit) {
        notices << tr("Przekroczono ustawiony limit wypełnienia.");
        critical = true;
    }
    if (result.unknownMassRows > 0) {
        notices << tr("%n wiersz nie ma masy katalogowej — pokazana masa jest "
                      "wartością minimalną, a nie pełną sumą.",
                      nullptr, result.unknownMassRows);
        critical = true;
    }
    if (result.unknownFireLoadRows > 0) {
        notices << tr("%n wiersz nie ma wartości obciążenia ogniowego — wynik MJ/m jest "
                      "wartością minimalną, a nie pełną sumą.",
                      nullptr, result.unknownFireLoadRows);
        critical = true;
    }
    if (result.estimatedFireLoadRows > 0) {
        notices << tr("* %n wiersz ma obciążenie ogniowe oszacowane z materiału "
                      "izolacji/powłoki i geometrii kabla. To wartość pomocnicza — "
                      "nie może być jedyną podstawą do rezygnacji z ochrony "
                      "przeciwpożarowej; potwierdź ją kartą producenta lub z "
                      "projektantem zabezpieczeń ppoż.",
                      nullptr, result.estimatedFireLoadRows);
        caution = true;
    }
    if (result.exceedsFireLoadLimit) {
        notices << tr("Przekroczono ustawiony limit obciążenia ogniowego.");
        critical = true;
    }
    if (notices.isEmpty()) {
        notices << tr("Dane są kompletne i nie przekraczają ustawionych progów.");
    }

    m_resultNotice->setText(notices.join(QStringLiteral("\n")));
    m_resultNotice->setStyleSheet(
        critical
            ? QStringLiteral("QLabel { color: #fecaca; background: #3f1d24; "
                             "border: 1px solid #7f1d1d; border-radius: 6px; padding: 10px; }")
            : caution
                ? QStringLiteral("QLabel { color: #fde68a; background: #3b2f17; "
                                 "border: 1px solid #a16207; border-radius: 6px; padding: 10px; }")
            : QStringLiteral("QLabel { color: #bbf7d0; background: #143322; "
                             "border: 1px solid #166534; border-radius: 6px; padding: 10px; }"));

    m_fillResult->setStyleSheet(result.exceedsFillLimit
                                    ? QStringLiteral("color: #fca5a5; font-weight: 700;")
                                    : QStringLiteral("color: #e2e8f0; font-weight: 700;"));
    m_fireLoadResult->setStyleSheet(
        result.exceedsFireLoadLimit || result.unknownFireLoadRows > 0
            ? QStringLiteral("color: #fca5a5; font-weight: 700;")
            : result.estimatedFireLoadRows > 0
                ? QStringLiteral("color: #fde68a; font-weight: 700;")
            : QStringLiteral("color: #e2e8f0; font-weight: 700;"));
    const QString massStyle =
        result.unknownMassRows > 0
            ? QStringLiteral("color: #fca5a5; font-weight: 700;")
            : QStringLiteral("color: #e2e8f0; font-weight: 700;");
    m_cableMassResult->setStyleSheet(massStyle);
    m_totalMassResult->setStyleSheet(massStyle);

    m_visualization->setProject(project);
}

void MainWindow::addFromCatalog()
{
    if (m_catalog.isEmpty()) {
        QMessageBox::information(
            this,
            tr("Katalog"),
            tr("Katalog jest pusty. Dodaj kabel własny lub uzupełnij bazę katalogową."));
        return;
    }

    CatalogDialog dialog(m_catalog, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto cable = dialog.selectedCable();
    if (cable.has_value()) {
        m_cableModel->addCable(cable.value());
    }
}

void MainWindow::addCustomCable()
{
    CableRow cable;
    cable.manufacturer = tr("Własny");
    cable.designation = tr("Nowy kabel");
    cable.quantity = 1;
    cable.outerDiameterMm = 10.0;
    cable.massKgPerKm = 100.0;
    m_cableModel->addCable(cable);

    const int row = m_cableModel->rowCount() - 1;
    m_cableTable->scrollTo(m_cableModel->index(row, 0));
    m_cableTable->setCurrentIndex(m_cableModel->index(row, 0));
    m_cableTable->edit(m_cableModel->index(row, 0));
}

void MainWindow::removeSelectedCables()
{
    const QModelIndexList selected = m_cableTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    QVector<int> rows;
    rows.reserve(selected.size());
    for (const auto &index : selected) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (const int row : rows) {
        m_cableModel->removeRow(row);
    }
}

void MainWindow::chooseBaksAssembly()
{
    if (m_baksCatalog.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Katalog BAKS"),
            tr("Wbudowany katalog produktów BAKS jest niedostępny."));
        return;
    }

    BaksAssemblyDialog dialog(
        m_baksCatalog,
        m_baksAssembly,
        m_suspensionHeight->value(),
        m_supportSpacing->value(),
        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_baksAssembly = dialog.assemblyItems();
    const BaksMassSummary summary =
        BaksCatalog::summarize(m_baksAssembly);

    m_updatingBaksFields = true;
    m_trayMass->setValue(summary.trayMassKgPerM);
    m_coverMass->setValue(summary.coverMassKgPerM);
    m_hangerBaseMass->setValue(summary.fixedMassKgPerSupport);
    m_verticalMass->setValue(summary.verticalMassKgPerSuspensionM);
    for (const auto &item : m_baksAssembly) {
        if (item.product.role == QStringLiteral("route")) {
            if (item.product.widthMm > 0.0) {
                m_width->setValue(item.product.widthMm);
            }
            if (item.product.heightMm > 0.0) {
                m_height->setValue(item.product.heightMm);
            }
            break;
        }
    }
    m_updatingBaksFields = false;

    updateBaksSummary();
    recalculate();
    statusBar()->showMessage(
        tr("Zastosowano zestaw BAKS z %n pozycją.", nullptr, m_baksAssembly.size()),
        7000);
}

void MainWindow::importXlsx()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Importuj projekt"),
        exportLocation(QString()),
        tr("Arkusze programu (*.xlsx)"));
    if (path.isEmpty()) {
        return;
    }

    ProjectData project;
    QString error;
    if (!XlsxProjectIo::importProject(path, &project, &error)) {
        QMessageBox::critical(this, tr("Import XLSX"), error);
        return;
    }

    if (!confirmUnsaved()) return;
    setProject(project);
    m_currentFile = path;
    m_dirty = m_autosavePending = false;
    setWindowModified(false);
    QFile::remove(recoveryPath());
    statusBar()->showMessage(tr("Zaimportowano %1").arg(path), 7000);
}

void MainWindow::exportXlsx()
{
    QString suggested = m_currentFile;
    if (suggested.isEmpty()) {
        const QString baseName = m_projectName->text().trimmed().isEmpty()
                                     ? QStringLiteral("trasa-kablowa")
                                     : m_projectName->text().trimmed();
        suggested = exportLocation(baseName + QStringLiteral(".xlsx"));
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Eksportuj projekt i raport"),
        suggested,
        tr("Arkusz Excel (*.xlsx)"));
    if (path.isEmpty()) {
        return;
    }

    const ProjectData project = currentProject();
    const CalculationResult result = Calculator::calculate(project);
    QString error;
    if (!XlsxProjectIo::exportProject(path, project, result, &error)) {
        QMessageBox::critical(this, tr("Eksport XLSX"), error);
        return;
    }

    m_currentFile = path;
    rememberExport(path);
    m_dirty = m_autosavePending = false;
    setWindowModified(false);
    QFile::remove(recoveryPath());
    statusBar()->showMessage(tr("Zapisano %1").arg(path), 7000);
}

void MainWindow::exportDxf()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Eksportuj przekrój i listę kabli"),
        exportLocation(QStringLiteral("trasa-kablowa.dxf")), tr("Rysunek CAD (*.dxf)"));
    if (path.isEmpty()) return;
    QString error;
    if (!DxfExport::write(path, currentProject(), &error)) {
        QMessageBox::critical(this, tr("Eksport DXF"), error);
        return;
    }
    rememberExport(path);
    statusBar()->showMessage(tr("Zapisano rysunek i tabelę kabli: %1").arg(path), 7000);
}

void MainWindow::newProject()
{
    if (!confirmUnsaved()) return;
    ProjectData project;
    setProject(project);
    m_currentFile.clear();
    m_dirty = m_autosavePending = false;
    setWindowModified(false);
    QFile::remove(recoveryPath());
    statusBar()->showMessage(tr("Utworzono nowy projekt."), 5000);
}

QString MainWindow::recoveryPath() const
{
    const QString dir = m_sessionDirectory.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : m_sessionDirectory;
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("autosave.json"));
}

QString MainWindow::exportLocation(const QString &name) const
{
    QString dir = QSettings().value(QStringLiteral("files/lastExportDirectory")).toString();
    if (!QDir(dir).exists() || dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(dir).filePath(name);
}

void MainWindow::rememberExport(const QString &path)
{
    QSettings().setValue(QStringLiteral("files/lastExportDirectory"), QFileInfo(path).absolutePath());
}

void MainWindow::autosave()
{
    if (!m_dirty || !m_autosavePending) return;
    QString error;
    if (!ProjectRecovery::save(recoveryPath(), currentProject(), m_currentFile, &error)) {
        statusBar()->showMessage(tr("Nie udało się zapisać kopii odzyskiwania: %1").arg(error));
        return;
    }
    m_autosavePending = false;
    statusBar()->showMessage(tr("Zapisano kopię odzyskiwania (%1).")
        .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))), 3000);
}

void MainWindow::recoverProject()
{
    const QString path = recoveryPath();
    if (!QFile::exists(path)) return;
    ProjectData project;
    QString original, error;
    if (!ProjectRecovery::load(path, &project, &original, &error)) {
        // Preserve corrupt copies for diagnostics instead of overwriting them.
        const QString backup = path + QStringLiteral(".damaged-") +
            QString::number(QDateTime::currentMSecsSinceEpoch());
        if (!QFile::rename(path, backup)) {
            QMessageBox::critical(this, tr("Odzyskiwanie"),
                tr("Nie można zabezpieczyć uszkodzonej kopii: %1. Program zostanie zamknięty.").arg(path));
            QApplication::quit();
            return;
        }
        QMessageBox::warning(this, tr("Odzyskiwanie"), error + QStringLiteral("\n") + backup);
        return;
    }
    if (QMessageBox::question(this, tr("Odzyskiwanie pracy"),
        tr("Znaleziono niezapisaną pracę z %1. Czy ją odzyskać? Wybranie Nie usuwa tę kopię.")
            .arg(QFileInfo(path).lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
        setProject(project);
        m_currentFile = original;
        m_dirty = true;
        setWindowModified(true);
    } else {
        QFile::remove(path);
    }
}

bool MainWindow::confirmUnsaved()
{
    // Finish any active cell editor before deciding whether the project is dirty.
    if (auto *focused = QApplication::focusWidget()) focused->clearFocus();
    if (!m_dirty) return true;
    autosave();
    const auto answer = QMessageBox::warning(this, tr("Niezapisane zmiany"),
        tr("Zapisać projekt do XLSX? Kopia automatyczna służy odzyskiwaniu, a DXF nie zastępuje projektu."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) { exportXlsx(); return !m_dirty; }
    return answer == QMessageBox::Discard;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmUnsaved()) { event->ignore(); return; }
    QFile::remove(recoveryPath());
    event->accept();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this,
        tr("O programie"),
        tr("<h3>Kalkulator Tras Kablowych</h3>"
           "<p>Wersja %1</p>"
           "<p>Wypełnienie jest liczone jako "
           "<b>Σ(ilość × D²) / (szerokość × wysokość)</b>.</p>"
           "<p>Obciążenie ogniowe wymaga potwierdzonych danych MJ/m. "
           "Klasa CPR nie jest automatycznie przeliczana na MJ/m.</p>"
           "<p>Wyniki wymagają weryfikacji przez uprawnionego projektanta "
           "i rzeczoznawcę ochrony przeciwpożarowej.</p>")
            .arg(QApplication::applicationVersion()));
}

void MainWindow::buildUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildProjectTab(), tr("1. Parametry trasy"));
    m_tabs->addTab(buildCablesTab(), tr("2. Lista kablowa"));
    m_tabs->addTab(buildResultsTab(), tr("3. Wyniki i przekrój"));
    setCentralWidget(m_tabs);
    buildMenus();
    statusBar()->showMessage(tr("Gotowy"));
}

QWidget *MainWindow::buildProjectTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 18, 22, 18);

    auto *identity = new QGroupBox(tr("Projekt"), page);
    auto *identityForm = new QFormLayout(identity);
    m_projectName = new QLineEdit(identity);
    m_projectName->setPlaceholderText(tr("np. Trasa TK-01 — pustka sufitowa"));
    identityForm->addRow(tr("Nazwa:"), m_projectName);
    layout->addWidget(identity);

    auto *columns = new QHBoxLayout;
    auto *routeGroup = new QGroupBox(tr("Geometria i progi"), page);
    auto *routeForm = new QFormLayout(routeGroup);
    m_width = makeSpin(1.0, 10000.0, 1, 300.0, tr(" mm"));
    m_height = makeSpin(1.0, 5000.0, 1, 60.0, tr(" mm"));
    m_fillLimit = makeSpin(0.0, 1000.0, 1, 40.0, tr(" %"));
    m_fireLimit = makeSpin(0.0, 1000000.0, 3, 0.0, tr(" MJ/m"));
    m_fireLimit->setSpecialValueText(tr("brak progu"));
    routeForm->addRow(tr("Szerokość wewnętrzna:"), m_width);
    routeForm->addRow(tr("Wysokość wewnętrzna:"), m_height);
    routeForm->addRow(tr("Limit wypełnienia:"), m_fillLimit);
    routeForm->addRow(tr("Próg obciążenia ogniowego:"), m_fireLimit);
    columns->addWidget(routeGroup, 1);

    auto *supportGroup = new QGroupBox(tr("Trasa i zawieszenia"), page);
    auto *supportForm = new QFormLayout(supportGroup);
    m_trayMass = makeSpin(0.0, 1000.0, 3, 0.0, tr(" kg/m"));
    m_coverMass = makeSpin(0.0, 1000.0, 3, 0.0, tr(" kg/m"));
    m_hangerBaseMass = makeSpin(0.0, 1000.0, 3, 0.0, tr(" kg/podporę"));
    m_suspensionHeight = makeSpin(0.0, 100.0, 3, 0.0, tr(" m"));
    m_verticalMass = makeSpin(0.0, 1000.0, 3, 0.0, tr(" kg/m zwieszenia"));
    m_supportSpacing = makeSpin(0.01, 100.0, 3, 1.5, tr(" m"));
    auto *baksButton = new QPushButton(tr("Dobierz zestaw z katalogu BAKS…"), supportGroup);
    baksButton->setToolTip(
        tr("Wybór koryta lub drabinki, pokrywy, wsporników, podstaw, "
           "zacisków i prętów gwintowanych według mas katalogowych BAKS."));
    m_baksSummary = new QLabel(supportGroup);
    m_baksSummary->setWordWrap(true);
    m_baksSummary->setProperty("role", "muted");
    supportForm->addRow(tr("Masa koryta:"), m_trayMass);
    supportForm->addRow(tr("Masa pokrywy:"), m_coverMass);
    supportForm->addRow(tr("Stała masa podpory:"), m_hangerBaseMass);
    supportForm->addRow(tr("Wysokość zwieszenia:"), m_suspensionHeight);
    supportForm->addRow(tr("Masa elementów pionowych:"), m_verticalMass);
    supportForm->addRow(tr("Rozstaw podpór:"), m_supportSpacing);
    supportForm->addRow(baksButton);
    supportForm->addRow(tr("Pochodzenie masy:"), m_baksSummary);
    connect(
        baksButton,
        &QPushButton::clicked,
        this,
        &MainWindow::chooseBaksAssembly);
    columns->addWidget(supportGroup, 1);
    layout->addLayout(columns);

    auto *note = new QLabel(
        tr("Masa trasy = koryto + pokrywa + "
           "(masa podpory + wysokość zwieszenia × masa elementów pionowych) / rozstaw podpór."),
        page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #9aa9bf; padding: 10px;"));
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::buildCablesTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *toolbar = new QHBoxLayout;
    auto *catalogButton = new QPushButton(tr("Dodaj z katalogu"), page);
    auto *customButton = new QPushButton(tr("Dodaj własny"), page);
    auto *removeButton = new QPushButton(tr("Usuń zaznaczone"), page);
    toolbar->addWidget(catalogButton);
    toolbar->addWidget(customButton);
    toolbar->addWidget(removeButton);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_cableTable = new QTableView(page);
    m_cableTable->setModel(m_cableModel);
    m_cableTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_cableTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_cableTable->setAlternatingRowColors(true);
    m_cableTable->setSortingEnabled(false);
    m_cableTable->verticalHeader()->setDefaultSectionSize(30);
    m_cableTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_cableTable->horizontalHeader()->setStretchLastSection(true);
    m_cableTable->setColumnWidth(CableTableModel::Manufacturer, 120);
    m_cableTable->setColumnWidth(CableTableModel::Designation, 220);
    m_cableTable->setColumnWidth(CableTableModel::Quantity, 70);
    m_cableTable->setColumnWidth(CableTableModel::Diameter, 90);
    m_cableTable->setColumnWidth(CableTableModel::Mass, 120);
    m_cableTable->setColumnWidth(CableTableModel::FireLoad, 190);
    m_cableTable->setColumnWidth(CableTableModel::CprClass, 130);
    layout->addWidget(m_cableTable, 1);

    auto *note = new QLabel(
        tr("Wartość MJ/m dotyczy jednej sztuki kabla. Puste pole jest traktowane jako "
           "brak danych, a nie jako zero."),
        page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #9aa9bf; padding: 6px;"));
    layout->addWidget(note);

    connect(catalogButton, &QPushButton::clicked, this, &MainWindow::addFromCatalog);
    connect(customButton, &QPushButton::clicked, this, &MainWindow::addCustomCable);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::removeSelectedCables);
    return page;
}

QWidget *MainWindow::buildResultsTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *splitter = new QSplitter(Qt::Horizontal, page);
    auto *summary = new QWidget(splitter);
    auto *summaryLayout = new QVBoxLayout(summary);

    auto *fillGroup = new QGroupBox(tr("Wypełnienie"), summary);
    auto *fillForm = new QFormLayout(fillGroup);
    m_reservedAreaResult = new QLabel(fillGroup);
    m_routeAreaResult = new QLabel(fillGroup);
    m_fillResult = new QLabel(fillGroup);
    fillForm->addRow(tr("Σ(ilość × D²):"), m_reservedAreaResult);
    fillForm->addRow(tr("Pole trasy:"), m_routeAreaResult);
    fillForm->addRow(tr("Wypełnienie:"), m_fillResult);
    summaryLayout->addWidget(fillGroup);

    auto *massGroup = new QGroupBox(tr("Obciążenie masowe"), summary);
    auto *massForm = new QFormLayout(massGroup);
    m_cableMassResult = new QLabel(massGroup);
    m_supportMassResult = new QLabel(massGroup);
    m_totalMassResult = new QLabel(massGroup);
    massForm->addRow(tr("Kable:"), m_cableMassResult);
    massForm->addRow(tr("Trasa i zawieszenia:"), m_supportMassResult);
    massForm->addRow(tr("Kompletna instalacja:"), m_totalMassResult);
    summaryLayout->addWidget(massGroup);

    auto *fireGroup = new QGroupBox(tr("Ochrona przeciwpożarowa"), summary);
    auto *fireForm = new QFormLayout(fireGroup);
    m_fireLoadResult = new QLabel(fireGroup);
    fireForm->addRow(tr("Łączne obciążenie ogniowe:"), m_fireLoadResult);
    summaryLayout->addWidget(fireGroup);

    m_resultNotice = new QLabel(summary);
    m_resultNotice->setWordWrap(true);
    summaryLayout->addWidget(m_resultNotice);
    summaryLayout->addStretch();

    m_visualization = new RouteVisualizationWidget(splitter);
    splitter->addWidget(summary);
    splitter->addWidget(m_visualization);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({360, 800});
    layout->addWidget(splitter, 1);
    auto *exportDxfButton = new QPushButton(tr("Eksportuj przekrój i tabelę do DXF…"), page);
    connect(exportDxfButton, &QPushButton::clicked, this, &MainWindow::exportDxf);
    layout->addWidget(exportDxfButton);
    return page;
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&Plik"));
    auto *newAction = fileMenu->addAction(tr("&Nowy projekt"));
    newAction->setShortcut(QKeySequence::New);
    auto *importAction = fileMenu->addAction(tr("&Importuj XLSX…"));
    importAction->setShortcut(QKeySequence::Open);
    auto *exportAction = fileMenu->addAction(tr("&Eksportuj XLSX…"));
    exportAction->setShortcut(QKeySequence::Save);
    auto *dxfAction = fileMenu->addAction(tr("Eksportuj rysunek DXF…"));
    connect(dxfAction, &QAction::triggered, this, &MainWindow::exportDxf);
    fileMenu->addSeparator();
    auto *exitAction = fileMenu->addAction(tr("Za&kończ"));
    exitAction->setShortcut(QKeySequence::Quit);

    auto *cableMenu = menuBar()->addMenu(tr("&Kable"));
    auto *catalogAction = cableMenu->addAction(tr("Dodaj z &katalogu…"));
    auto *customAction = cableMenu->addAction(tr("Dodaj &własny kabel"));

    auto *helpMenu = menuBar()->addMenu(tr("Pomo&c"));
    auto *aboutAction = helpMenu->addAction(tr("&O programie"));

    connect(newAction, &QAction::triggered, this, &MainWindow::newProject);
    connect(importAction, &QAction::triggered, this, &MainWindow::importXlsx);
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportXlsx);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(catalogAction, &QAction::triggered, this, &MainWindow::addFromCatalog);
    connect(customAction, &QAction::triggered, this, &MainWindow::addCustomCable);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

QDoubleSpinBox *MainWindow::makeSpin(
    double minimum,
    double maximum,
    int decimals,
    double value,
    const QString &suffix)
{
    auto *spin = new QDoubleSpinBox(this);
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setGroupSeparatorShown(true);
    spin->setKeyboardTracking(false);
    return spin;
}

ProjectData MainWindow::currentProject() const
{
    ProjectData project;
    project.route.projectName = m_projectName->text().trimmed();
    project.route.internalWidthMm = m_width->value();
    project.route.internalHeightMm = m_height->value();
    project.route.maximumFillPercent = m_fillLimit->value();
    project.route.fireLoadLimitMjPerM = m_fireLimit->value();
    project.route.trayMassKgPerM = m_trayMass->value();
    project.route.coverMassKgPerM = m_coverMass->value();
    project.route.hangerBaseMassKg = m_hangerBaseMass->value();
    project.route.suspensionHeightM = m_suspensionHeight->value();
    project.route.suspensionVerticalMassKgPerM = m_verticalMass->value();
    project.route.supportSpacingM = m_supportSpacing->value();
    project.cables = m_cableModel->cables();
    project.routeAssembly = m_baksAssembly;
    return project;
}

void MainWindow::setProject(const ProjectData &project)
{
    m_loading = true;
    m_updatingBaksFields = true;
    m_baksAssembly = project.routeAssembly;
    m_projectName->setText(project.route.projectName);
    m_width->setValue(project.route.internalWidthMm);
    m_height->setValue(project.route.internalHeightMm);
    m_fillLimit->setValue(project.route.maximumFillPercent);
    m_fireLimit->setValue(project.route.fireLoadLimitMjPerM);
    m_trayMass->setValue(project.route.trayMassKgPerM);
    m_coverMass->setValue(project.route.coverMassKgPerM);
    m_hangerBaseMass->setValue(project.route.hangerBaseMassKg);
    m_suspensionHeight->setValue(project.route.suspensionHeightM);
    m_verticalMass->setValue(project.route.suspensionVerticalMassKgPerM);
    m_supportSpacing->setValue(project.route.supportSpacingM);
    m_updatingBaksFields = false;
    updateBaksSummary();
    m_cableModel->setCables(project.cables);
    recalculate();
    m_loading = false;
}

void MainWindow::connectInputSignals()
{
    const QVector<QDoubleSpinBox *> spins = {
        m_width,
        m_height,
        m_fillLimit,
        m_fireLimit,
        m_trayMass,
        m_coverMass,
        m_hangerBaseMass,
        m_suspensionHeight,
        m_verticalMass,
        m_supportSpacing
    };
    for (auto *spin : spins) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, &MainWindow::recalculate);
    }
    const QVector<QDoubleSpinBox *> baksDerivedSpins = {
        m_trayMass,
        m_coverMass,
        m_hangerBaseMass,
        m_verticalMass
    };
    for (auto *spin : baksDerivedSpins) {
        connect(
            spin,
            &QDoubleSpinBox::valueChanged,
            this,
            &MainWindow::clearBaksAssembly);
    }
    connect(m_projectName, &QLineEdit::textChanged, this, &MainWindow::recalculate);
    connect(m_cableModel, &CableTableModel::cablesChanged, this, &MainWindow::recalculate);
}

void MainWindow::loadCatalog()
{
    QString error;
    if (!m_catalogRepository.open(&error)) {
        QMessageBox::warning(
            this,
            tr("Baza katalogowa"),
            tr("Nie udało się otworzyć bazy katalogowej:\n%1").arg(error));
        return;
    }
    m_catalog = m_catalogRepository.allItems(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Baza katalogowa"), error);
    } else {
        statusBar()->showMessage(
            tr("Załadowano %n pozycję katalogową.", nullptr, m_catalog.size()),
            5000);
    }
}

void MainWindow::loadBaksCatalog()
{
    QString error;
    m_baksCatalog = BaksCatalog::load(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Katalog BAKS"), error);
    }
    updateBaksSummary();
}

void MainWindow::updateBaksSummary()
{
    if (!m_baksSummary) {
        return;
    }
    if (m_baksAssembly.isEmpty()) {
        m_baksSummary->setText(tr("tryb ręczny"));
        m_baksSummary->setToolTip({});
        return;
    }

    QStringList symbols;
    QStringList sources;
    for (const auto &item : m_baksAssembly) {
        symbols << (item.quantity == 1.0
                        ? item.product.symbol
                        : QStringLiteral("%1 × %2")
                              .arg(item.quantity, 0, 'g', 4)
                              .arg(item.product.symbol));
        sources << QStringLiteral("%1 — %2%3\n%4")
                       .arg(
                           item.product.symbol,
                           item.product.sourceFile,
                           item.product.sourcePage > 0
                               ? tr(", strona PDF %1").arg(item.product.sourcePage)
                               : QString(),
                           item.product.sourceUrl);
    }
    m_baksSummary->setText(symbols.join(QStringLiteral(", ")));
    m_baksSummary->setToolTip(sources.join(QStringLiteral("\n\n")));
}

void MainWindow::clearBaksAssembly()
{
    if (m_updatingBaksFields || m_baksAssembly.isEmpty()) {
        return;
    }
    m_baksAssembly.clear();
    updateBaksSummary();
    statusBar()->showMessage(
        tr("Zmieniono masę ręcznie — powiązanie z zestawem BAKS usunięto."),
        5000);
}

void MainWindow::setResultLabel(
    QLabel *label,
    double value,
    int decimals,
    const QString &unit)
{
    label->setText(QStringLiteral("%1 %2")
                       .arg(QLocale().toString(value, 'f', decimals), unit));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

} // namespace ktk
