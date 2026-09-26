#include "ui/baksassemblydialog.h"

#include "domain/bakscatalog.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace ktk {

namespace {

void selectById(QComboBox *combo, const QString &id)
{
    const int index = combo->findData(id);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

BaksAssemblyDialog::BaksAssemblyDialog(
    const QVector<BaksProduct> &catalog,
    const QVector<RouteAssemblyItem> &currentItems,
    double suspensionHeightM,
    double supportSpacingM,
    QWidget *parent)
    : QDialog(parent)
    , m_catalog(catalog)
    , m_suspensionHeightM(std::max(0.0, suspensionHeightM))
    , m_supportSpacingM(std::max(0.001, supportSpacingM))
{
    for (const auto &item : currentItems) {
        if (item.product.role == QStringLiteral("route")) {
            m_initialRouteId = item.product.id;
        } else if (item.product.role == QStringLiteral("cover")) {
            m_initialCoverId = item.product.id;
        } else {
            m_supportItems.append(item);
        }
    }

    setWindowTitle(tr("Dobór masy trasy z katalogu BAKS"));
    resize(1000, 690);
    setMinimumSize(820, 560);
    buildUi();

    populateRouteOptions();
    selectById(m_routeCombo, m_initialRouteId);
    populateCoverOptions(m_initialCoverId);
    populateComponentOptions();
    rebuildComponentTable();
    componentChanged();
    updateSummary();
}

QVector<RouteAssemblyItem> BaksAssemblyDialog::assemblyItems() const
{
    QVector<RouteAssemblyItem> result;
    if (const auto route = selectedProduct(m_routeCombo)) {
        result.append(RouteAssemblyItem{*route, 1.0});
    }
    if (const auto cover = selectedProduct(m_coverCombo)) {
        result.append(RouteAssemblyItem{*cover, 1.0});
    }
    result += m_supportItems;
    return result;
}

void BaksAssemblyDialog::addComponent()
{
    const auto product = selectedProduct(m_componentCombo);
    if (!product.has_value()) {
        return;
    }

    const auto iterator = std::find_if(
        m_supportItems.begin(),
        m_supportItems.end(),
        [&product](const RouteAssemblyItem &item) {
            return item.product.id == product->id;
        });
    if (iterator == m_supportItems.end()) {
        m_supportItems.append(RouteAssemblyItem{*product, m_quantity->value()});
    } else {
        iterator->quantity += m_quantity->value();
    }
    rebuildComponentTable();
    updateSummary();
}

void BaksAssemblyDialog::removeSelectedComponents()
{
    const auto selected = m_componentTable->selectionModel()->selectedRows();
    QVector<int> rows;
    rows.reserve(selected.size());
    for (const auto &index : selected) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (const int row : rows) {
        if (row >= 0 && row < m_supportItems.size()) {
            m_supportItems.removeAt(row);
        }
    }
    rebuildComponentTable();
    updateSummary();
}

void BaksAssemblyDialog::routeChanged()
{
    const QString currentCover = m_coverCombo->currentData().toString();
    populateCoverOptions(currentCover);
    updateSummary();
}

void BaksAssemblyDialog::componentChanged()
{
    const auto product = selectedProduct(m_componentCombo);
    m_quantity->setValue(
        product.has_value() && product->role == QStringLiteral("vertical")
            ? 2.0
            : 1.0);
}

void BaksAssemblyDialog::updateSummary()
{
    const auto summary = BaksCatalog::summarize(assemblyItems());
    const double total =
        summary.trayMassKgPerM
        + summary.coverMassKgPerM
        + (summary.fixedMassKgPerSupport
           + m_suspensionHeightM
                 * summary.verticalMassKgPerSuspensionM)
            / m_supportSpacingM;

    m_summary->setText(
        tr("<b>Wynik zestawu:</b> %1 kg/m trasy<br>"
           "trasa %2 + pokrywa %3 + "
           "(elementy stałe %4 + wysokość %5 × pionowe %6) / rozstaw %7")
            .arg(total, 0, 'f', 3)
            .arg(summary.trayMassKgPerM, 0, 'f', 3)
            .arg(summary.coverMassKgPerM, 0, 'f', 3)
            .arg(summary.fixedMassKgPerSupport, 0, 'f', 3)
            .arg(m_suspensionHeightM, 0, 'f', 3)
            .arg(summary.verticalMassKgPerSuspensionM, 0, 'f', 3)
            .arg(m_supportSpacingM, 0, 'f', 3));
}

void BaksAssemblyDialog::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *description = new QLabel(
        tr("Wybierz elementy według symboli BAKS. Masa koryta, drabinki i "
           "pokrywy jest liczona na metr trasy. Elementy stałe są mnożone "
           "przez liczbę na punkt podparcia, a pręty przez wysokość zwieszenia."),
        this);
    description->setWordWrap(true);
    description->setProperty("role", "muted");
    layout->addWidget(description);

    auto *continuousGroup = new QGroupBox(tr("Elementy ciągłe"), this);
    auto *continuousForm = new QFormLayout(continuousGroup);
    m_routeCombo = new QComboBox(continuousGroup);
    m_routeCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_routeCombo->setMinimumContentsLength(46);
    m_coverCombo = new QComboBox(continuousGroup);
    m_coverCombo->setMinimumContentsLength(46);
    continuousForm->addRow(tr("Koryto / drabinka:"), m_routeCombo);
    continuousForm->addRow(tr("Pokrywa:"), m_coverCombo);
    layout->addWidget(continuousGroup);

    auto *supportGroup = new QGroupBox(tr("Elementy montażowe na punkt podparcia"), this);
    auto *supportLayout = new QVBoxLayout(supportGroup);
    auto *addRow = new QHBoxLayout;
    m_componentCombo = new QComboBox(supportGroup);
    m_componentCombo->setMinimumContentsLength(42);
    m_quantity = new QDoubleSpinBox(supportGroup);
    m_quantity->setRange(0.01, 1000.0);
    m_quantity->setDecimals(2);
    m_quantity->setValue(1.0);
    m_quantity->setSuffix(tr(" szt."));
    auto *addButton = new QPushButton(tr("Dodaj element"), supportGroup);
    auto *removeButton = new QPushButton(tr("Usuń zaznaczone"), supportGroup);
    addRow->addWidget(m_componentCombo, 1);
    addRow->addWidget(m_quantity);
    addRow->addWidget(addButton);
    addRow->addWidget(removeButton);
    supportLayout->addLayout(addRow);

    m_componentTable = new QTableWidget(supportGroup);
    m_componentTable->setColumnCount(6);
    m_componentTable->setHorizontalHeaderLabels(
        {tr("Symbol"),
         tr("Nr katalogowy"),
         tr("Sposób liczenia"),
         tr("Ilość"),
         tr("Wkład masowy"),
         tr("Źródło")});
    m_componentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_componentTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_componentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_componentTable->setAlternatingRowColors(true);
    m_componentTable->horizontalHeader()->setStretchLastSection(true);
    m_componentTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    supportLayout->addWidget(m_componentTable, 1);
    layout->addWidget(supportGroup, 1);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setStyleSheet(
        QStringLiteral("QLabel { color: #bfdbfe; background: #15233a; "
                       "border: 1px solid #31527c; border-radius: 6px; padding: 10px; }"));
    layout->addWidget(m_summary);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Zastosuj zestaw"));
    layout->addWidget(buttons);

    connect(addButton, &QPushButton::clicked, this, &BaksAssemblyDialog::addComponent);
    connect(
        removeButton,
        &QPushButton::clicked,
        this,
        &BaksAssemblyDialog::removeSelectedComponents);
    connect(
        m_routeCombo,
        &QComboBox::currentIndexChanged,
        this,
        &BaksAssemblyDialog::routeChanged);
    connect(
        m_coverCombo,
        &QComboBox::currentIndexChanged,
        this,
        &BaksAssemblyDialog::updateSummary);
    connect(
        m_componentCombo,
        &QComboBox::currentIndexChanged,
        this,
        &BaksAssemblyDialog::componentChanged);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void BaksAssemblyDialog::populateRouteOptions()
{
    m_routeCombo->clear();
    m_routeCombo->addItem(tr("— brak / masa ręczna —"), QString());
    for (const auto &product : m_catalog) {
        if (product.role == QStringLiteral("route")) {
            m_routeCombo->addItem(productLabel(product), product.id);
        }
    }
}

void BaksAssemblyDialog::populateCoverOptions(const QString &preferredId)
{
    const auto route = selectedProduct(m_routeCombo);
    const double width = route.has_value() ? route->widthMm : 0.0;

    m_coverCombo->blockSignals(true);
    m_coverCombo->clear();
    m_coverCombo->addItem(tr("— bez pokrywy / masa ręczna —"), QString());
    for (const auto &product : m_catalog) {
        if (product.role != QStringLiteral("cover")) {
            continue;
        }
        if (width > 0.0 && product.widthMm != width) {
            continue;
        }
        m_coverCombo->addItem(productLabel(product), product.id);
    }
    selectById(m_coverCombo, preferredId);
    m_coverCombo->blockSignals(false);
}

void BaksAssemblyDialog::populateComponentOptions()
{
    m_componentCombo->clear();
    for (const auto &product : m_catalog) {
        if (product.role == QStringLiteral("fixed")
            || product.role == QStringLiteral("vertical")) {
            m_componentCombo->addItem(
                QStringLiteral("[%1] %2")
                    .arg(BaksCatalog::roleLabel(product.role), productLabel(product)),
                product.id);
        }
    }
}

void BaksAssemblyDialog::rebuildComponentTable()
{
    m_componentTable->setRowCount(m_supportItems.size());
    for (int row = 0; row < m_supportItems.size(); ++row) {
        const auto &item = m_supportItems.at(row);
        const auto &product = item.product;
        const QString source =
            product.sourcePage > 0
                ? tr("PDF s. %1").arg(product.sourcePage)
                : tr("karta online");
        const QString tooltip =
            QStringLiteral("%1\n%2")
                .arg(product.sourceFile, product.sourceUrl);
        const QStringList values = {
            product.symbol,
            product.catalogCode,
            BaksCatalog::roleLabel(product.role),
            QString::number(item.quantity, 'f', 2),
            BaksCatalog::contributionLabel(item),
            source
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *cell = new QTableWidgetItem(values.at(column));
            cell->setToolTip(tooltip);
            m_componentTable->setItem(row, column, cell);
        }
    }
}

std::optional<BaksProduct> BaksAssemblyDialog::selectedProduct(
    const QComboBox *combo) const
{
    const QString id = combo->currentData().toString();
    if (id.isEmpty()) {
        return std::nullopt;
    }
    return BaksCatalog::findById(m_catalog, id);
}

QString BaksAssemblyDialog::productLabel(const BaksProduct &product) const
{
    QString geometry;
    if (product.widthMm > 0.0) {
        geometry = product.heightMm > 0.0
                       ? QStringLiteral(" | %1 × H%2 mm")
                             .arg(product.widthMm, 0, 'f', 0)
                             .arg(product.heightMm, 0, 'f', 0)
                       : QStringLiteral(" | %1 mm")
                             .arg(product.widthMm, 0, 'f', 0);
    }
    return QStringLiteral("%1 | kat. %2%3 | %4 %5")
        .arg(
            product.symbol,
            product.catalogCode,
            geometry,
            QString::number(product.massKgPerUnit, 'f', 3),
            product.massUnit);
}

} // namespace ktk
