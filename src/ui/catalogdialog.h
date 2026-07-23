#pragma once

#include "domain/types.h"

#include <QDialog>

class QLineEdit;
class QTableWidget;

namespace ktk {

class CatalogDialog final : public QDialog {
    Q_OBJECT

public:
    explicit CatalogDialog(const QVector<CatalogItem> &items, QWidget *parent = nullptr);
    [[nodiscard]] std::optional<CableRow> selectedCable() const;

private:
    void rebuildTable(const QString &filter);

    QVector<CatalogItem> m_items;
    QLineEdit *m_filter = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace ktk
