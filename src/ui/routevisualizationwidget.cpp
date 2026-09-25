#include "ui/routevisualizationwidget.h"
#include "domain/cablelayout.h"

#include <QLocale>
#include <QPainter>

#include <algorithm>

namespace ktk {

RouteVisualizationWidget::RouteVisualizationWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setToolTip(tr("Schemat obliczeniowy: kable są sortowane malejąco według średnicy "
                  "i układane warstwami od dna trasy."));
}

void RouteVisualizationWidget::setProject(const ProjectData &project)
{
    m_project = project;
    update();
}

QSize RouteVisualizationWidget::minimumSizeHint() const
{
    return {520, 320};
}

void RouteVisualizationWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#0b1220")));

    const double widthMm = m_project.route.internalWidthMm;
    const double heightMm = m_project.route.internalHeightMm;
    if (widthMm <= 0.0 || heightMm <= 0.0) {
        painter.setPen(QColor(QStringLiteral("#9aa9bf")));
        painter.drawText(rect(), Qt::AlignCenter, tr("Podaj dodatnie wymiary trasy."));
        return;
    }

    const QRectF available = QRectF(rect()).adjusted(48.0, 40.0, -48.0, -54.0);
    const double scale = std::min(available.width() / widthMm, available.height() / heightMm);
    const QSizeF traySize(widthMm * scale, heightMm * scale);
    const QRectF tray(
        available.center().x() - traySize.width() / 2.0,
        available.bottom() - traySize.height(),
        traySize.width(),
        traySize.height());

    painter.setPen(QPen(QColor(QStringLiteral("#64748b")), 3.0));
    painter.setBrush(QColor(QStringLiteral("#111827")));
    painter.drawRect(tray);

    const auto placed = layoutCables(tray);
    for (const auto &cable : placed) {
        painter.setBrush(cable.overflow
                             ? QColor(QStringLiteral("#7f1d1d"))
                             : cable.color);
        painter.setPen(QPen(cable.overflow
                                ? QColor(QStringLiteral("#fca5a5"))
                                : QColor(QStringLiteral("#cbd5e1")),
                            cable.overflow ? 2.5 : 1.0));
        painter.drawEllipse(cable.rectangle);

        if (cable.rectangle.width() >= 34.0) {
            painter.setPen(cable.overflow
                               ? QColor(QStringLiteral("#fff1f2"))
                               : QColor(QStringLiteral("#0b1220")));
            QFont font = painter.font();
            font.setPixelSize(std::clamp(
                static_cast<int>(cable.rectangle.width() / 5.0), 8, 12));
            painter.setFont(font);
            painter.drawText(cable.rectangle.adjusted(2, 2, -2, -2),
                             Qt::AlignCenter | Qt::TextWordWrap, cable.label);
        }
    }

    painter.setPen(QColor(QStringLiteral("#9aa9bf")));
    painter.drawText(
        QRectF(tray.left(), tray.bottom() + 10.0, tray.width(), 24.0),
        Qt::AlignCenter,
        tr("%1 × %2 mm — schemat, nie instrukcja montażowa")
            .arg(QLocale().toString(widthMm, 'f', 0))
            .arg(QLocale().toString(heightMm, 'f', 0)));
}

QVector<RouteVisualizationWidget::PlacedCable>
RouteVisualizationWidget::layoutCables(const QRectF &tray) const
{
    const double scale = tray.width() / m_project.route.internalWidthMm;
    QVector<PlacedCable> result;
    for (const auto &c : cableLayout(m_project)) {
        const auto &row = m_project.cables[c.row];
        result.append({QRectF(tray.left() + c.x * scale,
                             tray.bottom() - (c.y + c.diameter) * scale,
                             c.diameter * scale, c.diameter * scale),
                       row.designation, cableColor(row), c.overflow});
    }
    return result;
}

QColor RouteVisualizationWidget::cableColor(const CableRow &cable)
{
    const uint hash = qHash(cable.manufacturer + cable.designation);
    return QColor::fromHsv(static_cast<int>(hash % 360U), 95, 225, 210);
}

} // namespace ktk
