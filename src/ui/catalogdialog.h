#pragma once

#include "domain/types.h"

#include <QDialog>

class QLineEdit;
class QLabel;
class QComboBox;
class QTableWidget;
class QTimer;

namespace ktk {

class CatalogDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CatalogDialog(const QVector<CatalogItem> &items, QWidget *parent = nullptr);
    [[nodiscard]] std::optional<CableRow> selectedCable() const;

private:
    void populateFilters();
    void rebuildTable();
    void clearFilters();

    QVector<CatalogItem> m_items;
    QLineEdit *m_filter = nullptr;
    QComboBox *m_manufacturerFilter = nullptr;
    QComboBox *m_typeFilter = nullptr;
    QComboBox *m_insulationFilter = nullptr;
    QComboBox *m_cprFilter = nullptr;
    QComboBox *m_fireResistanceFilter = nullptr;
    QTimer *m_searchDelay = nullptr;
    QLabel *m_summary = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace ktk
