#include "domain/catalogfilter.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ktk {

namespace {

QString folded(QString text)
{
    text = text.toLower();
    static const QList<QPair<QString, QString>> replacements = {
        {QStringLiteral("ą"), QStringLiteral("a")},
        {QStringLiteral("ć"), QStringLiteral("c")},
        {QStringLiteral("ę"), QStringLiteral("e")},
        {QStringLiteral("ł"), QStringLiteral("l")},
        {QStringLiteral("ń"), QStringLiteral("n")},
        {QStringLiteral("ó"), QStringLiteral("o")},
        {QStringLiteral("ś"), QStringLiteral("s")},
        {QStringLiteral("ź"), QStringLiteral("z")},
        {QStringLiteral("ż"), QStringLiteral("z")}
    };
    for (const auto &[source, target] : replacements) {
        text.replace(source, target);
    }

    static const QRegularExpression decimalComma(
        QStringLiteral(R"((?<=\d),(?=\d))"));
    static const QRegularExpression conductorSeparator(
        QStringLiteral(R"((?<=\d)\s*[x×g]\s*(?=\d))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression separators(QStringLiteral(R"([^a-z0-9.]+)"));

    text.replace(decimalComma, QStringLiteral("."));
    text.replace(conductorSeparator, QStringLiteral(" "));
    text.replace(separators, QStringLiteral(" "));
    return text.simplified();
}

bool numericToken(const QString &token, double *value = nullptr)
{
    static const QRegularExpression numeric(
        QStringLiteral(R"(^\d+(?:\.\d+)?$)"));
    if (!numeric.match(token).hasMatch()) {
        return false;
    }
    bool ok = false;
    const double parsed = token.toDouble(&ok);
    if (ok && value) {
        *value = parsed;
    }
    return ok;
}

QStringList tokensFromText(const QString &text)
{
    QStringList tokens = folded(text).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    tokens.removeAll(QStringLiteral("x"));
    tokens.removeAll(QStringLiteral("g"));
    return tokens;
}

bool tokenListsMatch(const QStringList &required, const QString &text)
{
    if (required.isEmpty()) {
        return true;
    }
    const QStringList available = tokensFromText(text);

    return std::all_of(
        required.cbegin(),
        required.cend(),
        [&available](const QString &needle) {
            double requestedNumber = 0.0;
            if (numericToken(needle, &requestedNumber)) {
                return std::any_of(
                    available.cbegin(),
                    available.cend(),
                    [requestedNumber](const QString &candidate) {
                        double candidateNumber = 0.0;
                        return numericToken(candidate, &candidateNumber)
                            && std::abs(candidateNumber - requestedNumber) < 1e-9;
                    });
            }
            return std::any_of(
                available.cbegin(),
                available.cend(),
                [&needle](const QString &candidate) {
                    return candidate.contains(needle);
                });
        });
}

QString normalizedMaterialText(const CatalogItem &item)
{
    return folded(item.cable.designation).toUpper().replace(
        QRegularExpression(QStringLiteral(R"([^A-Z0-9]+)")),
        QStringLiteral(" "));
}

bool containsTag(const QStringList &tags, const QString &selected)
{
    return std::any_of(tags.cbegin(), tags.cend(), [&selected](const QString &tag) {
        return tag.compare(selected, Qt::CaseInsensitive) == 0;
    });
}

} // namespace

CatalogFilter::CatalogFilter(CatalogFilterCriteria criteria)
    : m_criteria(std::move(criteria))
    , m_queryTokens(tokensFromText(m_criteria.query))
    , m_cableTypeTokens(tokensFromText(m_criteria.cableType))
{
}

QStringList CatalogFilter::queryTokens(const QString &text)
{
    return tokensFromText(text);
}

bool CatalogFilter::tokenizedMatch(const QString &query, const QString &text)
{
    return tokenListsMatch(tokensFromText(query), text);
}

QString CatalogFilter::cableFamily(const CatalogItem &item)
{
    QString family = item.cable.designation.trimmed();
    static const QRegularExpression conductorConfiguration(
        QStringLiteral(R"(\b\d+\s*(?:x|×|g)\s*\d)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = conductorConfiguration.match(family);
    if (match.hasMatch()) {
        family.truncate(match.capturedStart());
    }
    family = family.trimmed();
    while (family.endsWith(QLatin1Char('-'))
           || family.endsWith(QLatin1Char('/'))
           || family.endsWith(QLatin1Char(','))) {
        family.chop(1);
        family = family.trimmed();
    }
    return family.isEmpty() ? item.cable.designation.trimmed() : family;
}

QStringList CatalogFilter::insulationTags(const CatalogItem &item)
{
    const QString text = normalizedMaterialText(item);
    QStringList tags;

    static const QRegularExpression pvcPattern(
        QStringLiteral(
            R"(\b(?:YKY|YAKY|YDY|YADY|YKSY|YKSLY|YSLY|YLY|YTKSY|LIYY|H0[357]VV|H0[57]V|OMY|OWY)[A-Z0-9]*\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression xlpePattern(
        QStringLiteral(
            R"(\b(?:N2X|NA2X|N2XH|N2XCH|NHXH|NHXCH|YKXS|YAKXS|XRUHA|A2XS|AALXS)[A-Z0-9]*\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression polyethylenePattern(
        QStringLiteral(R"(\b(?:LI2Y|2Y|XzTKMXpw|XzTKMXpwn)[A-Z0-9]*\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression halogenFreePattern(
        QStringLiteral(
            R"(\b(?:N2XH|N2XCH|NHXH|NHXCH|H0[157]Z|HTKSH|HDGS|JE H|LSZH|LSOH|LS0H)[A-Z0-9]*\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rubberPattern(
        QStringLiteral(
            R"(\b(?:H0[57]RN|H07BB|NSSH|NSHT|NSGAF|H01N2|ONPD|OGL)[A-Z0-9]*\b)"),
        QRegularExpression::CaseInsensitiveOption);

    const bool pvc = pvcPattern.match(text).hasMatch();
    const bool xlpe = xlpePattern.match(text).hasMatch();
    const bool polyethylene = polyethylenePattern.match(text).hasMatch();
    const bool halogenFree = halogenFreePattern.match(text).hasMatch();
    const bool rubber = rubberPattern.match(text).hasMatch();

    if (pvc) {
        tags.append(QStringLiteral("PVC"));
    }
    if (xlpe) {
        tags.append(QStringLiteral("XLPE / polietylen sieciowany"));
    }
    if (polyethylene) {
        tags.append(QStringLiteral("PE / polietylen"));
    }
    if (halogenFree) {
        tags.append(QStringLiteral("bezhalogenowa (LSZH)"));
    }
    if (rubber) {
        tags.append(QStringLiteral("guma / elastomer"));
    }
    return tags;
}

QStringList CatalogFilter::fireResistanceTags(const CatalogItem &item)
{
    const QString text =
        (item.cable.designation + QLatin1Char(' ') + item.notes).toUpper();
    QStringList tags;

    static const QRegularExpression phOrFe(
        QStringLiteral(R"((?<![A-Z0-9])(PH|FE)\s*-?\s*(\d{2,3})(?!\d))"));
    auto iterator = phOrFe.globalMatch(text);
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        tags.append(match.captured(1) + match.captured(2));
    }

    static const QRegularExpression eClass(
        QStringLiteral(R"((?<![A-Z0-9])E(30|60|90|120)(?!\d))"));
    iterator = eClass.globalMatch(text);
    while (iterator.hasNext()) {
        tags.append(QStringLiteral("E") + iterator.next().captured(1));
    }

    tags.removeDuplicates();
    tags.sort(Qt::CaseInsensitive);
    return tags;
}

bool CatalogFilter::matches(const CatalogItem &item) const
{
    const QString searchable =
        item.cable.manufacturer + QLatin1Char(' ')
        + item.cable.designation + QLatin1Char(' ')
        + item.cable.catalogCode + QLatin1Char(' ')
        + item.cable.cprClass;

    if (!tokenListsMatch(m_queryTokens, searchable)) {
        return false;
    }
    if (!m_criteria.manufacturer.isEmpty()
        && item.cable.manufacturer.compare(
               m_criteria.manufacturer,
               Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (!m_cableTypeTokens.isEmpty()
        && !tokenListsMatch(m_cableTypeTokens, cableFamily(item))) {
        return false;
    }
    if (!m_criteria.insulation.isEmpty()
        && !containsTag(insulationTags(item), m_criteria.insulation)) {
        return false;
    }
    if (!m_criteria.cprClass.isEmpty()
        && item.cable.cprClass.compare(
               m_criteria.cprClass,
               Qt::CaseInsensitive) != 0) {
        return false;
    }
    if (!m_criteria.fireResistance.isEmpty()
        && !containsTag(
            fireResistanceTags(item),
            m_criteria.fireResistance)) {
        return false;
    }
    return true;
}

bool CatalogFilter::matches(
    const CatalogItem &item,
    const CatalogFilterCriteria &criteria)
{
    return CatalogFilter(criteria).matches(item);
}

} // namespace ktk
