#pragma once

#include "domain/types.h"

#include <QColor>
#include <QRectF>
#include <QWidget>

namespace ktk {

class RouteVisualizationWidget final : public QWidget {
    Q_OBJECT

public:
    explicit RouteVisualizationWidget(QWidget *parent = nullptr);
    void setProject(const ProjectData &project);

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct PlacedCable {
        QRectF rectangle;
        QString label;
        QColor color;
        bool overflow = false;
    };

    [[nodiscard]] QVector<PlacedCable> layoutCables(const QRectF &tray) const;
    [[nodiscard]] static QColor cableColor(const CableRow &cable);

    ProjectData m_project;
};

} // namespace ktk
