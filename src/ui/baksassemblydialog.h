#pragma once

#include "domain/types.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTableWidget;

namespace ktk {

class BaksAssemblyDialog final : public QDialog {
    Q_OBJECT

public:
    explicit BaksAssemblyDialog(
        const QVector<BaksProduct> &catalog,
        const QVector<RouteAssemblyItem> &currentItems,
        double suspensionHeightM,
        double supportSpacingM,
        QWidget *parent = nullptr);

    [[nodiscard]] QVector<RouteAssemblyItem> assemblyItems() const;

private slots:
    void addComponent();
    void removeSelectedComponents();
    void routeChanged();
    void componentChanged();
    void updateSummary();

private:
    void buildUi();
    void populateRouteOptions();
    void populateCoverOptions(const QString &preferredId = {});
    void populateComponentOptions();
    void rebuildComponentTable();
    [[nodiscard]] std::optional<BaksProduct> selectedProduct(
        const QComboBox *combo) const;
    [[nodiscard]] QString productLabel(const BaksProduct &product) const;

    QVector<BaksProduct> m_catalog;
    QVector<RouteAssemblyItem> m_supportItems;
    QString m_initialRouteId;
    QString m_initialCoverId;
    double m_suspensionHeightM = 0.0;
    double m_supportSpacingM = 1.5;

    QComboBox *m_routeCombo = nullptr;
    QComboBox *m_coverCombo = nullptr;
    QComboBox *m_componentCombo = nullptr;
    QDoubleSpinBox *m_quantity = nullptr;
    QTableWidget *m_componentTable = nullptr;
    QLabel *m_summary = nullptr;
};

} // namespace ktk
