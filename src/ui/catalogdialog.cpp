#include "ui/catalogdialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ktk {

namespace {

constexpr int MaximumDisplayedRows = 1000;

} // namespace

CatalogDialog::CatalogDialog(const QVector<CatalogItem> &items, QWidget *parent)
    : QDialog(parent)
    , m_items(items)
{
    setWindowTitle(tr("Dodaj kabel z katalogu"));
    resize(1120, 560);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Wyszukaj producenta, oznaczenie lub CPR:"), this));

    m_filter = new QLineEdit(this);
    m_filter->setClearButtonEnabled(true);
    m_filter->setPlaceholderText(tr("np. ELPAR 1 x 10"));
    layout->addWidget(m_filter);

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

    connect(m_filter, &QLineEdit::textChanged, this, &CatalogDialog::rebuildTable);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { accept(); });

    rebuildTable({});
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

void CatalogDialog::rebuildTable(const QString &filter)
{
    const QString needle = filter.trimmed();
    m_table->setUpdatesEnabled(false);
    m_table->setRowCount(0);
    int matches = 0;

    for (int i = 0; i < m_items.size(); ++i) {
        const auto &entry = m_items.at(i);
        const QString haystack =
            entry.cable.manufacturer + ' ' + entry.cable.designation + ' '
            + entry.cable.catalogCode + ' ' + entry.cable.cprClass;
        if (!needle.isEmpty() && !haystack.contains(needle, Qt::CaseInsensitive)) {
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
        m_table->setItem(row, 5, new QTableWidgetItem(
            entry.cable.fireLoadMjPerM.has_value()
                ? QLocale().toString(entry.cable.fireLoadMjPerM.value(), 'f', 3)
                : tr("brak")));
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
