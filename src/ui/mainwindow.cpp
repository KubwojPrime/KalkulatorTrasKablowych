#include "ui/mainwindow.h"

#include "domain/calculator.h"
#include "io/xlsxprojectio.h"
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
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

namespace ktk {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 780);
    setMinimumSize(980, 640);
    setWindowTitle(tr("Kalkulator Tras Kablowych"));

    m_cableModel = new CableTableModel(this);
    buildUi();
    loadCatalog();
    connectInputSignals();
    recalculate();
}

void MainWindow::showAccessNotice(const QString &message)
{
    statusBar()->showMessage(message, 15000);
}

void MainWindow::recalculate()
{
    const ProjectData project = currentProject();
    const CalculationResult result = Calculator::calculate(project);

    setResultLabel(m_reservedAreaResult, result.reservedCableAreaMm2, 1, tr("mm²"));
    setResultLabel(m_routeAreaResult, result.routeAreaMm2, 1, tr("mm²"));
    setResultLabel(m_fillResult, result.fillPercent, 2, tr("%"));
    setResultLabel(m_cableMassResult, result.cableMassKgPerM, 3, tr("kg/m"));
    setResultLabel(m_supportMassResult, result.supportSystemMassKgPerM, 3, tr("kg/m"));
    setResultLabel(m_totalMassResult, result.totalInstalledMassKgPerM, 3, tr("kg/m"));
    setResultLabel(m_fireLoadResult, result.knownFireLoadMjPerM, 3, tr("MJ/m"));

    QStringList notices;
    bool critical = false;
    if (result.invalidRows > 0) {
        notices << tr("%n nieprawidłowy wiersz został pominięty.",
                      nullptr, result.invalidRows);
        critical = true;
    }
    if (result.exceedsFillLimit) {
        notices << tr("Przekroczono ustawiony limit wypełnienia.");
        critical = true;
    }
    if (result.unknownFireLoadRows > 0) {
        notices << tr("%n wiersz nie ma wartości obciążenia ogniowego — wynik MJ/m jest "
                      "wartością minimalną, a nie pełną sumą.",
                      nullptr, result.unknownFireLoadRows);
        critical = true;
    } else if (result.exceedsFireLoadLimit) {
        notices << tr("Przekroczono ustawiony limit obciążenia ogniowego.");
        critical = true;
    }
    if (notices.isEmpty()) {
        notices << tr("Dane są kompletne i nie przekraczają ustawionych progów.");
    }

    m_resultNotice->setText(notices.join(QStringLiteral("\n")));
    m_resultNotice->setStyleSheet(
        critical
            ? QStringLiteral("QLabel { color: #991b1b; background: #fee2e2; "
                             "border: 1px solid #fecaca; border-radius: 6px; padding: 10px; }")
            : QStringLiteral("QLabel { color: #166534; background: #dcfce7; "
                             "border: 1px solid #bbf7d0; border-radius: 6px; padding: 10px; }"));

    m_fillResult->setStyleSheet(result.exceedsFillLimit
                                    ? QStringLiteral("color: #b91c1c; font-weight: 700;")
                                    : QStringLiteral("color: #0f172a; font-weight: 700;"));
    m_fireLoadResult->setStyleSheet(
        result.exceedsFireLoadLimit || result.unknownFireLoadRows > 0
            ? QStringLiteral("color: #b91c1c; font-weight: 700;")
            : QStringLiteral("color: #0f172a; font-weight: 700;"));

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

void MainWindow::importXlsx()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Importuj projekt"),
        {},
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

    setProject(project);
    m_currentFile = path;
    statusBar()->showMessage(tr("Zaimportowano %1").arg(path), 7000);
}

void MainWindow::exportXlsx()
{
    QString suggested = m_currentFile;
    if (suggested.isEmpty()) {
        const QString baseName = m_projectName->text().trimmed().isEmpty()
                                     ? QStringLiteral("trasa-kablowa")
                                     : m_projectName->text().trimmed();
        suggested = baseName + QStringLiteral(".xlsx");
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
    statusBar()->showMessage(tr("Zapisano %1").arg(path), 7000);
}

void MainWindow::newProject()
{
    ProjectData project;
    setProject(project);
    m_currentFile.clear();
    statusBar()->showMessage(tr("Utworzono nowy projekt."), 5000);
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
    supportForm->addRow(tr("Masa koryta:"), m_trayMass);
    supportForm->addRow(tr("Masa pokrywy:"), m_coverMass);
    supportForm->addRow(tr("Stała masa podpory:"), m_hangerBaseMass);
    supportForm->addRow(tr("Wysokość zwieszenia:"), m_suspensionHeight);
    supportForm->addRow(tr("Masa elementów pionowych:"), m_verticalMass);
    supportForm->addRow(tr("Rozstaw podpór:"), m_supportSpacing);
    columns->addWidget(supportGroup, 1);
    layout->addLayout(columns);

    auto *note = new QLabel(
        tr("Masa trasy = koryto + pokrywa + "
           "(masa podpory + wysokość zwieszenia × masa elementów pionowych) / rozstaw podpór."),
        page);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: #475569; padding: 10px;"));
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
    note->setStyleSheet(QStringLiteral("color: #475569; padding: 6px;"));
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
    fireForm->addRow(tr("Znane obciążenie ogniowe:"), m_fireLoadResult);
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
    return project;
}

void MainWindow::setProject(const ProjectData &project)
{
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
    m_cableModel->setCables(project.cables);
    recalculate();
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
