#include "ui/catalogdialog.h"

#include "domain/catalogfilter.h"

#include <QComboBox>
#include <QCompleter>
#include <QColor>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace ktk {

namespace {

constexpr int MaximumDisplayedRows = 1000;

void addOptions(QComboBox *combo, const QString &anyLabel, QStringList values)
{
    values.removeAll(QString());
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);

    combo->addItem(anyLabel, QString());
    for (const auto &value : values) {
        combo->addItem(value, value);
    }
}

QString selectedValue(const QComboBox *combo, bool allowTypedValue = false)
{
    if (allowTypedValue && combo->currentIndex() < 0) {
        return combo->currentText().trimmed();
    }
    if (combo->currentIndex() <= 0) {
        return {};
    }
    return combo->currentData().toString().trimmed();
}

void configureCompactCombo(QComboBox *combo)
{
    combo->setMinimumContentsLength(18);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
}

} // namespace

CatalogDialog::CatalogDialog(const QVector<CatalogItem> &items, QWidget *parent)
    : QDialog(parent)
    , m_items(items)
{
    setWindowTitle(tr("Dodaj kabel z katalogu"));
    resize(1280, 680);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Wyszukiwanie bazowe — wszystkie wpisane elementy muszą wystąpić:"),
        this));

    m_filter = new QLineEdit(this);
    m_filter->setClearButtonEnabled(true);
    m_filter->setPlaceholderText(
        tr("np. YKY 3 x 2,5 — kolejność i znaki rozdzielające nie mają znaczenia"));
    layout->addWidget(m_filter);

    auto *filterGrid = new QGridLayout;
    m_manufacturerFilter = new QComboBox(this);
    m_typeFilter = new QComboBox(this);
    m_insulationFilter = new QComboBox(this);
    m_cprFilter = new QComboBox(this);
    m_fireResistanceFilter = new QComboBox(this);
    for (auto *combo : {
             m_manufacturerFilter,
             m_typeFilter,
             m_insulationFilter,
             m_cprFilter,
             m_fireResistanceFilter}) {
        configureCompactCombo(combo);
    }

    m_typeFilter->setEditable(true);
    m_typeFilter->setInsertPolicy(QComboBox::NoInsert);

    filterGrid->addWidget(new QLabel(tr("Producent:"), this), 0, 0);
    filterGrid->addWidget(m_manufacturerFilter, 1, 0);
    filterGrid->addWidget(new QLabel(tr("Typ / rodzina kabla:"), this), 0, 1);
    filterGrid->addWidget(m_typeFilter, 1, 1);
    filterGrid->addWidget(new QLabel(tr("Izolacja / powłoka:"), this), 0, 2);
    filterGrid->addWidget(m_insulationFilter, 1, 2);
    filterGrid->addWidget(new QLabel(tr("Klasa CPR:"), this), 0, 3);
    filterGrid->addWidget(m_cprFilter, 1, 3);
    filterGrid->addWidget(new QLabel(tr("Odporność ogniowa:"), this), 0, 4);
    filterGrid->addWidget(m_fireResistanceFilter, 1, 4);

    auto *clearButton = new QPushButton(tr("Wyczyść filtry"), this);
    filterGrid->addWidget(clearButton, 1, 5);
    filterGrid->setColumnStretch(1, 2);
    layout->addLayout(filterGrid);

    auto *help = new QLabel(
        tr("Wyszukiwanie łączy słowa operatorem AND. Materiał izolacji/powłoki "
           "jest rozpoznawany z oznaczenia kabla i powinien być potwierdzony "
           "w karcie producenta. * w kolumnie MJ/m oznacza konserwatywne "
           "oszacowanie materiałowe, a nie wartość producenta."),
        this);
    help->setWordWrap(true);
    help->setProperty("role", QStringLiteral("muted"));
    layout->addWidget(help);

    m_summary = new QLabel(this);
    layout->addWidget(m_summary);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels({
        tr("Producent"),
        tr("Oznaczenie"),
        tr("Kod katalogowy"),
        tr("D [mm]"),
        tr("Masa [kg/km]"),
        tr("MJ/m"),
        tr("CPR"),
        tr("Weryfikacja")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Dodaj"));
    layout->addWidget(buttons);

    populateFilters();

    m_searchDelay = new QTimer(this);
    m_searchDelay->setSingleShot(true);
    m_searchDelay->setInterval(180);
    connect(m_searchDelay, &QTimer::timeout, this, &CatalogDialog::rebuildTable);
    connect(m_filter, &QLineEdit::textChanged, m_searchDelay, qOverload<>(&QTimer::start));
    connect(
        m_typeFilter,
        &QComboBox::currentTextChanged,
        m_searchDelay,
        qOverload<>(&QTimer::start));
    for (auto *combo : {
             m_manufacturerFilter,
             m_insulationFilter,
             m_cprFilter,
             m_fireResistanceFilter}) {
        connect(combo, &QComboBox::currentTextChanged, this, [this] {
            rebuildTable();
        });
    }
    connect(clearButton, &QPushButton::clicked, this, &CatalogDialog::clearFilters);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { accept(); });

    rebuildTable();
}

void CatalogDialog::populateFilters()
{
    QStringList manufacturers;
    QStringList types;
    QStringList insulationTypes;
    QStringList cprClasses;
    QStringList fireResistanceClasses;

    for (const auto &item : m_items) {
        manufacturers.append(item.cable.manufacturer.trimmed());
        types.append(CatalogFilter::cableFamily(item));
        insulationTypes.append(CatalogFilter::insulationTags(item));
        cprClasses.append(item.cable.cprClass.trimmed());
        fireResistanceClasses.append(
            CatalogFilter::fireResistanceTags(item));
    }

    addOptions(m_manufacturerFilter, tr("Dowolny producent"), manufacturers);
    addOptions(m_typeFilter, tr("Dowolny typ"), types);
    addOptions(m_insulationFilter, tr("Dowolny materiał"), insulationTypes);
    addOptions(m_cprFilter, tr("Dowolna klasa CPR"), cprClasses);
    addOptions(
        m_fireResistanceFilter,
        tr("Dowolna odporność"),
        fireResistanceClasses);

    if (auto *completer = m_typeFilter->completer()) {
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
    }
}

std::optional<CableRow> CatalogDialog::selectedCable() const
{
    const int row = m_table->currentRow();
    if (row < 0) {
        return std::nullopt;
    }
    const auto *item = m_table->item(row, 0);
    if (!item) {
        return std::nullopt;
    }
    const int sourceIndex = item->data(Qt::UserRole).toInt();
    if (sourceIndex < 0 || sourceIndex >= m_items.size()) {
        return std::nullopt;
    }
    return m_items.at(sourceIndex).cable;
}

void CatalogDialog::clearFilters()
{
    m_searchDelay->stop();
    const QSignalBlocker queryBlocker(m_filter);
    const QSignalBlocker manufacturerBlocker(m_manufacturerFilter);
    const QSignalBlocker typeBlocker(m_typeFilter);
    const QSignalBlocker insulationBlocker(m_insulationFilter);
    const QSignalBlocker cprBlocker(m_cprFilter);
    const QSignalBlocker fireBlocker(m_fireResistanceFilter);

    m_filter->clear();
    m_manufacturerFilter->setCurrentIndex(0);
    m_typeFilter->setCurrentIndex(0);
    m_insulationFilter->setCurrentIndex(0);
    m_cprFilter->setCurrentIndex(0);
    m_fireResistanceFilter->setCurrentIndex(0);
    rebuildTable();
}

void CatalogDialog::rebuildTable()
{
    CatalogFilterCriteria criteria;
    criteria.query = m_filter->text();
    criteria.manufacturer = selectedValue(m_manufacturerFilter);
    criteria.cableType = selectedValue(m_typeFilter, true);
    criteria.insulation = selectedValue(m_insulationFilter);
    criteria.cprClass = selectedValue(m_cprFilter);
    criteria.fireResistance = selectedValue(m_fireResistanceFilter);
    const CatalogFilter matcher(criteria);

    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    int matches = 0;

    for (int i = 0; i < m_items.size(); ++i) {
        const auto &entry = m_items.at(i);
        if (!matcher.matches(entry)) {
            continue;
        }
        ++matches;
        if (m_table->rowCount() >= MaximumDisplayedRows) {
            continue;
        }

        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto *manufacturer = new QTableWidgetItem(entry.cable.manufacturer);
        manufacturer->setData(Qt::UserRole, i);
        QStringList provenance = {
            entry.notes,
            entry.cable.source,
            entry.sourceUrl,
            entry.extractionMethod
        };
        provenance.removeAll(QString());
        manufacturer->setToolTip(provenance.join(QLatin1Char('\n')));
        m_table->setItem(row, 0, manufacturer);
        m_table->setItem(row, 1, new QTableWidgetItem(entry.cable.designation));
        m_table->setItem(row, 2, new QTableWidgetItem(entry.cable.catalogCode));
        m_table->setItem(row, 3, new QTableWidgetItem(
            QLocale().toString(entry.cable.outerDiameterMm, 'f', 2)));
        m_table->setItem(row, 4, new QTableWidgetItem(
            entry.cable.massKgPerKm.has_value()
                ? QLocale().toString(entry.cable.massKgPerKm.value(), 'f', 2)
                : tr("brak")));
        auto *fireLoad = new QTableWidgetItem(
            entry.cable.fireLoadMjPerM.has_value()
                ? QLocale().toString(entry.cable.fireLoadMjPerM.value(), 'f', 3)
                    + (entry.cable.fireLoadEstimated
                           ? QStringLiteral("*")
                           : QString())
                : tr("brak"));
        if (entry.cable.fireLoadEstimated) {
            fireLoad->setToolTip(entry.cable.fireLoadBasis);
            fireLoad->setForeground(QColor(QStringLiteral("#fde68a")));
            fireLoad->setBackground(QColor(QStringLiteral("#3b2f17")));
        }
        m_table->setItem(row, 5, fireLoad);
        m_table->setItem(row, 6, new QTableWidgetItem(entry.cable.cprClass));
        m_table->setItem(row, 7, new QTableWidgetItem(
            entry.verified ? tr("sprawdzone") : tr("do weryfikacji")));
    }

    m_summary->setText(
        tr("Znaleziono: %1. Wyświetlono: %2%3")
            .arg(matches)
            .arg(m_table->rowCount())
            .arg(matches > MaximumDisplayedRows
                     ? tr(" (zawęź wyszukiwanie, aby zobaczyć pozostałe)")
                     : QString()));
    if (m_table->rowCount() > 0) {
        m_table->selectRow(0);
    }
    m_table->setUpdatesEnabled(true);
}

} // namespace ktk
