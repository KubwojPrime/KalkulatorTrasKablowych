#include "ui/cabletablemodel.h"

#include <QBrush>
#include <QLocale>

namespace ktk {

namespace {

QString displayNumber(double value, int precision)
{
    return QLocale().toString(value, 'f', precision);
}

} // namespace

CableTableModel::CableTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CableTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_cables.size();
}

int CableTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant CableTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_cables.size()) {
        return {};
    }

    const auto &cable = m_cables.at(index.row());
    if (role == Qt::BackgroundRole
        && (cable.quantity <= 0 || cable.outerDiameterMm <= 0.0 || cable.massKgPerKm < 0.0)) {
        return QBrush(QColor(QStringLiteral("#ffe4e6")));
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == FireLoad && !cable.fireLoadMjPerM.has_value()) {
            return tr("Brak potwierdzonej wartości MJ/m. Wiersz nie jest dodawany do sumy "
                      "obciążenia ogniowego.");
        }
        if (index.column() == Source) {
            return cable.source;
        }
    }
    if (role != Qt::DisplayRole && role != Qt::EditRole) {
        return {};
    }

    const bool edit = role == Qt::EditRole;
    switch (index.column()) {
    case Manufacturer:
        return cable.manufacturer;
    case Designation:
        return cable.designation;
    case Quantity:
        return cable.quantity;
    case Diameter:
        return edit ? QVariant(cable.outerDiameterMm) : QVariant(displayNumber(cable.outerDiameterMm, 2));
    case Mass:
        return edit ? QVariant(cable.massKgPerKm) : QVariant(displayNumber(cable.massKgPerKm, 2));
    case FireLoad:
        if (!cable.fireLoadMjPerM.has_value()) {
            return edit ? QVariant() : QVariant(tr("brak danych"));
        }
        return edit ? QVariant(cable.fireLoadMjPerM.value())
                    : QVariant(displayNumber(cable.fireLoadMjPerM.value(), 3));
    case CprClass:
        return cable.cprClass;
    case Source:
        return cable.source;
    default:
        return {};
    }
}

QVariant CableTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return {};
    }
    if (orientation == Qt::Vertical) {
        return section + 1;
    }

    switch (section) {
    case Manufacturer:
        return tr("Producent");
    case Designation:
        return tr("Typ / przekrój");
    case Quantity:
        return tr("Ilość");
    case Diameter:
        return tr("D [mm]");
    case Mass:
        return tr("Masa [kg/km]");
    case FireLoad:
        return tr("Obciążenie ogniowe [MJ/m]");
    case CprClass:
        return tr("CPR");
    case Source:
        return tr("Źródło");
    default:
        return {};
    }
}

Qt::ItemFlags CableTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

bool CableTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid()
        || index.row() < 0 || index.row() >= m_cables.size()) {
        return false;
    }

    auto &cable = m_cables[index.row()];
    bool ok = true;
    switch (index.column()) {
    case Manufacturer:
        cable.manufacturer = value.toString().trimmed();
        break;
    case Designation:
        cable.designation = value.toString().trimmed();
        break;
    case Quantity: {
        const int count = value.toInt(&ok);
        if (ok) {
            cable.quantity = count;
        }
        break;
    }
    case Diameter:
        cable.outerDiameterMm = localizedDouble(value, &ok);
        break;
    case Mass:
        cable.massKgPerKm = localizedDouble(value, &ok);
        break;
    case FireLoad:
        if (value.toString().trimmed().isEmpty()) {
            cable.fireLoadMjPerM.reset();
        } else {
            const double parsed = localizedDouble(value, &ok);
            if (ok) {
                cable.fireLoadMjPerM = parsed;
            }
        }
        break;
    case CprClass:
        cable.cprClass = value.toString().trimmed();
        break;
    case Source:
        cable.source = value.toString().trimmed();
        break;
    default:
        return false;
    }

    if (!ok) {
        return false;
    }

    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, Qt::BackgroundRole});
    emit cablesChanged();
    return true;
}

bool CableTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || count <= 0 || row + count > m_cables.size()) {
        return false;
    }

    beginRemoveRows({}, row, row + count - 1);
    m_cables.remove(row, count);
    endRemoveRows();
    emit cablesChanged();
    return true;
}

void CableTableModel::addCable(const CableRow &cable)
{
    const int row = m_cables.size();
    beginInsertRows({}, row, row);
    m_cables.append(cable);
    endInsertRows();
    emit cablesChanged();
}

void CableTableModel::setCables(const QVector<CableRow> &cables)
{
    beginResetModel();
    m_cables = cables;
    endResetModel();
    emit cablesChanged();
}

const QVector<CableRow> &CableTableModel::cables() const
{
    return m_cables;
}

double CableTableModel::localizedDouble(const QVariant &value, bool *ok)
{
    if (value.canConvert<double>() && value.metaType().id() != QMetaType::QString) {
        return value.toDouble(ok);
    }

    const QString text = value.toString().trimmed();
    double parsed = QLocale().toDouble(text, ok);
    if (!*ok) {
        parsed = QLocale::c().toDouble(QString(text).replace(',', '.'), ok);
    }
    return parsed;
}

} // namespace ktk
