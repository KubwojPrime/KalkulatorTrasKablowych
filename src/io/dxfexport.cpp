#include "io/dxfexport.h"
#include "domain/cablelayout.h"
#include "domain/calculator.h"
#include <QSaveFile>
#include <QTextStream>
#include <QLocale>
#include <cmath>
#include <algorithm>

namespace ktk {
namespace {
QString cadText(const QString &value)
{
    QString result;
    for (QChar c : value) {
        const auto u = c.unicode();
        if (u < 32) result += QLatin1Char(' ');
        else result += c;
    }
    return result;
}
QString number(double n) { return QString::number(n, 'f', 3); }
}

bool DxfExport::write(const QString &path, const ProjectData &p, QString *error)
{
    auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    const double w = p.route.internalWidthMm, h = p.route.internalHeightMm;
    if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0)
        return fail(QStringLiteral("Podaj dodatnie, skończone wymiary trasy."));
    qint64 count = 0;
    for (const auto &c : p.cables) {
        if (c.quantity <= 0 || !std::isfinite(c.outerDiameterMm) || c.outerDiameterMm <= 0)
            return fail(QStringLiteral("Popraw ilości i średnice kabli przed eksportem."));
        count += c.quantity;
        for (auto value : {c.massKgPerKm, c.fireLoadMjPerM})
            if (value && (!std::isfinite(*value) || *value < 0))
                return fail(QStringLiteral("Nieprawidłowa masa lub obciążenie ogniowe kabla."));
    }
    if (count > 100000) return fail(QStringLiteral("Eksport obsługuje maksymalnie 100 000 kabli."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setLocale(QLocale::c());
    out.setRealNumberPrecision(12);
    quint64 handle = 16;
    auto pair = [&](int code, const auto &v) {
        out << code << '\n' << v << '\n';
        if (code == 0) {
            QString value;
            QTextStream stream(&value);
            stream << v;
            if (QStringList{"TABLE", "LTYPE", "LAYER", "STYLE", "LINE", "TEXT", "CIRCLE"}.contains(value))
                out << "5\n" << QString::number(handle++, 16).toUpper() << '\n';
        }
    };
    pair(0, "SECTION"); pair(2, "HEADER");
    pair(9, "$ACADVER"); pair(1, "AC1021");
    pair(9, "$DWGCODEPAGE"); pair(3, "ANSI_1252");
    pair(9, "$INSUNITS"); pair(70, 4);
    pair(9, "$MEASUREMENT"); pair(70, 1);
    pair(0, "ENDSEC"); pair(0, "SECTION"); pair(2, "TABLES");
    pair(0, "TABLE"); pair(2, "LTYPE"); pair(100, "AcDbSymbolTable"); pair(70, 1);
    pair(0, "LTYPE"); pair(2, "CONTINUOUS"); pair(70, 0); pair(3, "Solid line");
    pair(72, 65); pair(73, 0); pair(40, 0); pair(0, "ENDTAB");
    pair(0, "TABLE"); pair(2, "LAYER"); pair(100, "AcDbSymbolTable"); pair(70, 6);
    for (const auto &layer : {"0", "TRASA", "KABLE", "PRZEPELNIENIE", "OPISY", "TABELA"}) {
        pair(0, "LAYER"); pair(2, layer); pair(70, 0);
        pair(62, QString::fromLatin1(layer) == QStringLiteral("PRZEPELNIENIE") ? 1 : 7);
        pair(6, "CONTINUOUS");
    }
    pair(0, "ENDTAB");
    pair(0, "TABLE"); pair(2, "STYLE"); pair(100, "AcDbSymbolTable"); pair(70, 1);
    pair(0, "STYLE"); pair(2, "STANDARD"); pair(70, 0); pair(40, 0);
    pair(41, 1); pair(50, 0); pair(71, 0); pair(42, 2.5);
    pair(3, "arial.ttf"); pair(4, ""); pair(0, "ENDTAB");
    pair(0, "ENDSEC"); pair(0, "SECTION"); pair(2, "ENTITIES");
    auto line = [&](double x1, double y1, double x2, double y2, const char *layer) {
        pair(0, "LINE"); pair(100, "AcDbEntity"); pair(8, layer);
        pair(100, "AcDbLine"); pair(10, x1); pair(20, y1); pair(30, 0);
        pair(11, x2); pair(21, y2); pair(31, 0);
    };
    auto text = [&](double x, double y, double size, const QString &s, const char *layer) {
        pair(0, "TEXT"); pair(100, "AcDbEntity"); pair(8, layer);
        pair(100, "AcDbText"); pair(10, x); pair(20, y); pair(30, 0);
        pair(40, size); pair(1, cadText(s)); pair(7, "STANDARD");
        pair(100, "AcDbText");
    };
    line(0, 0, w, 0, "TRASA"); line(0, 0, 0, h, "TRASA");
    line(w, 0, w, h, "TRASA"); line(0, h, w, h, "TRASA");
    double top = h;
    for (const auto &c : cableLayout(p)) {
        const double r = c.diameter / 2;
        const char *layer = c.overflow ? "PRZEPELNIENIE" : "KABLE";
        pair(0, "CIRCLE"); pair(100, "AcDbEntity"); pair(8, layer);
        pair(100, "AcDbCircle"); pair(10, c.x + r); pair(20, c.y + r);
        pair(30, 0); pair(40, r);
        const QString label = QString::number(c.row + 1);
        const double size = c.diameter / (2.0 * std::max(2, int(label.size())));
        text(c.x + r - size * label.size() * 0.3, c.y + r - size / 2,
             size, label, "OPISY");
        top = std::max(top, c.y + c.diameter);
    }
    text(0, top + 15, 4, p.route.projectName, "OPISY");
    text(0, top + 7, 3, QStringLiteral("Przekrój %1 x %2 mm; jednostka: mm, skala 1:1")
         .arg(number(w), number(h)), "OPISY");
    text(0, -8, 2.5, QStringLiteral("Schemat obliczeniowy, nie instrukcja montażowa. Czerwony: przekroczenie obrysu."), "OPISY");
    const auto result = Calculator::calculate(p);
    text(0, -15, 2.5, QStringLiteral("Wypełnienie (suma n x D²): %1%; masa kabli: %2%3 kg/m; ogień: %4%5%6 MJ/m")
         .arg(number(result.fillPercent), result.unknownMassRows ? ">= " : "",
              number(result.cableMassKgPerM), result.unknownFireLoadRows ? ">= " : "",
              number(result.knownFireLoadMjPerM), result.estimatedFireLoadRows ? "*" : ""), "OPISY");
    QVector<QStringList> table;
    table.append({"Lp.", "Producent", "Oznaczenie kabla", "Kod katalogowy", "Ilość", "D [mm]", "kg/km", "MJ/m kabla", "CPR"});
    for (int i = 0; i < p.cables.size(); ++i) {
        const auto &c = p.cables[i];
        table.append({QString::number(i + 1), c.manufacturer, c.designation, c.catalogCode,
            QString::number(c.quantity), number(c.outerDiameterMm),
            c.massKgPerKm ? number(*c.massKgPerKm) : QStringLiteral("brak danych"),
            c.fireLoadMjPerM ? number(*c.fireLoadMjPerM) + (c.fireLoadEstimated ? "*" : "") : QStringLiteral("brak danych"), c.cprClass});
    }
    QVector<double> widths(9, 18);
    for (const auto &row : table)
        for (int j = 0; j < row.size(); ++j)
            widths[j] = std::max(widths[j], row[j].size() * 2.5 + 6.0);
    double totalWidth = 0;
    for (double width : widths) totalWidth += width;
    const double tableTop = -25, bottom = tableTop - table.size() * 8;
    for (int i = 0; i <= table.size(); ++i)
        line(0, tableTop - i * 8, totalWidth, tableTop - i * 8, "TABELA");
    double x = 0;
    for (int j = 0; j < widths.size(); ++j) {
        line(x, tableTop, x, bottom, "TABELA");
        for (int i = 0; i < table.size(); ++i)
            text(x + 2, tableTop - i * 8 - 5.5, 2.5, table[i][j], "TABELA");
        x += widths[j];
    }
    line(x, tableTop, x, bottom, "TABELA");
    text(0, bottom - 8, 2.5, QStringLiteral("* Obciążenie na podstawie materiału izolacji/powłoki (oszacowanie)."), "OPISY");
    text(0, bottom - 15, 2.5, QStringLiteral("Brak danych nie oznacza zera. Wynik ogniowy wymaga weryfikacji projektowej."), "OPISY");
    pair(0, "ENDSEC"); pair(0, "EOF");
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) return fail(file.errorString());
    return true;
}
}
