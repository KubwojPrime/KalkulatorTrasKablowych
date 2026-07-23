#pragma once

#include "domain/types.h"

#include <QAbstractTableModel>

namespace ktk {

class CableTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        Manufacturer = 0,
        Designation,
        Quantity,
        Diameter,
        Mass,
        FireLoad,
        CprClass,
        Source,
        ColumnCount
    };

    explicit CableTableModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    bool removeRows(int row, int count, const QModelIndex &parent = {}) override;

    void addCable(const CableRow &cable);
    void setCables(const QVector<CableRow> &cables);
    [[nodiscard]] const QVector<CableRow> &cables() const;

signals:
    void cablesChanged();

private:
    [[nodiscard]] static double localizedDouble(const QVariant &value, bool *ok);
    QVector<CableRow> m_cables;
};

} // namespace ktk
