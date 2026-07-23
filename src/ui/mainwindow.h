#pragma once

#include "data/catalogrepository.h"
#include "domain/types.h"

#include <QMainWindow>

class QAction;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableView;
class QTabWidget;

namespace ktk {

class CableTableModel;
class RouteVisualizationWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void showAccessNotice(const QString &message);

private slots:
    void recalculate();
    void addFromCatalog();
    void addCustomCable();
    void removeSelectedCables();
    void importXlsx();
    void exportXlsx();
    void newProject();
    void showAbout();

private:
    void buildUi();
    QWidget *buildProjectTab();
    QWidget *buildCablesTab();
    QWidget *buildResultsTab();
    void buildMenus();
    [[nodiscard]] QDoubleSpinBox *makeSpin(
        double minimum,
        double maximum,
        int decimals,
        double value,
        const QString &suffix = {});
    [[nodiscard]] ProjectData currentProject() const;
    void setProject(const ProjectData &project);
    void connectInputSignals();
    void loadCatalog();
    void setResultLabel(QLabel *label, double value, int decimals, const QString &unit);

    CatalogRepository m_catalogRepository;
    QVector<CatalogItem> m_catalog;
    CableTableModel *m_cableModel = nullptr;

    QTabWidget *m_tabs = nullptr;
    QLineEdit *m_projectName = nullptr;
    QDoubleSpinBox *m_width = nullptr;
    QDoubleSpinBox *m_height = nullptr;
    QDoubleSpinBox *m_fillLimit = nullptr;
    QDoubleSpinBox *m_fireLimit = nullptr;
    QDoubleSpinBox *m_trayMass = nullptr;
    QDoubleSpinBox *m_coverMass = nullptr;
    QDoubleSpinBox *m_hangerBaseMass = nullptr;
    QDoubleSpinBox *m_suspensionHeight = nullptr;
    QDoubleSpinBox *m_verticalMass = nullptr;
    QDoubleSpinBox *m_supportSpacing = nullptr;

    QTableView *m_cableTable = nullptr;
    RouteVisualizationWidget *m_visualization = nullptr;

    QLabel *m_reservedAreaResult = nullptr;
    QLabel *m_routeAreaResult = nullptr;
    QLabel *m_fillResult = nullptr;
    QLabel *m_cableMassResult = nullptr;
    QLabel *m_supportMassResult = nullptr;
    QLabel *m_totalMassResult = nullptr;
    QLabel *m_fireLoadResult = nullptr;
    QLabel *m_resultNotice = nullptr;

    QString m_currentFile;
};

} // namespace ktk
