#!/usr/bin/env python3
"""Extract auditable cable variants from the archived official PDF catalogs.

The script intentionally accepts only rows for which an external diameter can be
associated with a concrete product variant. Missing mass and fire-load values stay
null. It never derives fire load from CPR.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
from collections import defaultdict
from dataclasses import dataclass
from datetime import date
from pathlib import Path
from typing import Any, Iterable

import pdfplumber


DATASET_VERSION = "2026-07-24.1"
NUMBER_RE = re.compile(r"\d+(?:[.,]\d+)?")
CPR_RE = re.compile(
    r"(?<![A-Za-z0-9])"
    r"(?P<class>[ABCDEF](?:1|2)?ca"
    r"(?:[-–][sd]\d[a-z]?(?:\s*,\s*[dsa]\d[a-z]?){0,3})?)",
    re.IGNORECASE,
)
CATALOG_CODE_RE = re.compile(r"^[A-Z0-9][A-Z0-9./_-]{2,}$", re.IGNORECASE)
COBI_CODE_RE = re.compile(
    r"\bC\d{2}-\d{3,6}(?:-\d{2,4})?\b",
    re.IGNORECASE,
)
CID_RE = re.compile(r"\(cid:(\d+)\)")
BITNER_CID_OVERRIDES = {
    225: "ł",
    261: "ć",
    266: "ę",
    276: "ń",
    286: "Ś",
    298: "ż",
}
BITNER_HEADING_OVERRIDES = {
    107: "BiT 1000 (St) OR",
    111: "BiT 1000 2(St) OR",
}


@dataclass(frozen=True)
class SourceSpec:
    source_id: str
    parser: str
    primary: bool = True
    priority: int = 0


SOURCE_SPECS = (
    SourceSpec("telefonika-power-2026", "anchored", priority=30),
    SourceSpec("telefonika-telecom-2025", "anchored", priority=20),
    SourceSpec("telefonika-cpr-2026", "supplemental", primary=False),
    SourceSpec("elpar-power", "anchored", priority=20),
    SourceSpec("elpar-telecom", "anchored", priority=20),
    SourceSpec("elpar-control-special", "anchored", priority=20),
    SourceSpec("elpar-safe-halogen-free", "anchored", priority=20),
    SourceSpec("elpar-power-technical-parameters", "supplemental", primary=False),
    SourceSpec("elpar-fire-safety-parameters", "supplemental", primary=False),
    SourceSpec("bitner-general-2025", "bitner", priority=30),
    SourceSpec("bitner-bitlan-2025", "supplemental", primary=False),
    SourceSpec("cobicabling-catalog-pl", "cobi", priority=10),
    SourceSpec("cobicabling-catalog-en-2025", "cobi", priority=30),
)


def compact_space(value: str) -> str:
    return re.sub(r"\s+", " ", value.replace("\u00a0", " ")).strip()


def decode_bitner_cid(value: str) -> str:
    def replacement(match: re.Match[str]) -> str:
        code = int(match.group(1))
        if code in BITNER_CID_OVERRIDES:
            return BITNER_CID_OVERRIDES[code]
        shifted = code + 29
        if 32 <= shifted <= 126:
            return chr(shifted)
        # Non-ASCII glyphs occur mainly in descriptive headers. Keeping a visible
        # marker is safer than silently fabricating a Polish character.
        return "?"

    return CID_RE.sub(replacement, value)


def normalized_unit(value: str) -> str:
    return (
        compact_space(value)
        .lower()
        .replace("[", "")
        .replace("]", "")
        .replace("²", "2")
        .replace("^", "")
        .replace("×", "x")
        .replace(" ", "")
    )


def parsed_numbers(value: str) -> list[float]:
    result: list[float] = []
    for match in NUMBER_RE.finditer(value):
        try:
            result.append(float(match.group(0).replace(",", ".")))
        except ValueError:
            pass
    return result


def maximum_dimension(value: str) -> float | None:
    values = parsed_numbers(value)
    if not values:
        return None
    if "±" in value and len(values) >= 2:
        return values[0] + values[1]
    if any(separator in value for separator in ("x", "X", "×", "-", "–", "÷")):
        return max(values)
    return values[0]


def parse_mass(value: str) -> float | None:
    values = parsed_numbers(value)
    if not values:
        return None
    if "-" in value or "–" in value:
        return max(values)
    return values[0]


def format_decimal(value: float) -> str:
    if math.isclose(value, round(value), abs_tol=1e-9):
        return str(int(round(value)))
    return f"{value:g}".replace(".", ",")


def normalize_section(value: str, unit_suffix: str = "mm²") -> str | None:
    value = compact_space(value)
    value = value.replace("×", "x").replace("X", "x")
    value = re.sub(
        r"\s*(?:\[?\s*mm(?:²|2)\s*\]?)\s*$",
        "",
        value,
        flags=re.IGNORECASE,
    )
    value = re.sub(r"\s*x\s*", " x ", value)
    value = re.sub(r"\s+", " ", value).strip(" ,;–-")
    if not value or not any(character.isdigit() for character in value):
        return None
    lowered = normalized_unit(value)
    if lowered in {"mm", "mm2", "kg/km"}:
        return None
    if len(value) > 70:
        return None
    if re.fullmatch(r"\d+(?:[.,]\d+)?", value):
        number = maximum_dimension(value)
        if number is None or number <= 0 or number > 10000:
            return None
        return f"1 x {format_decimal(number)} {unit_suffix}"
    has_protective_core_notation = bool(
        re.search(r"\d+\s*[Gg]\s*\d+(?:[.,]\d+)?", value)
    )
    if (
        "x" not in value
        and "/" not in value
        and "+" not in value
        and not has_protective_core_notation
    ):
        return None
    return f"{value} {unit_suffix}"


def approximate_metal_area(section: str) -> float | None:
    compact = section.replace(" ", "").replace(",", ".")
    matches = re.findall(r"(\d+)[xGg](\d+(?:\.\d+)?)", compact)
    if not matches:
        return None
    return sum(float(count) * float(area) for count, area in matches)


def plausible_cable_dimensions(section: str, diameter: float, mass: float) -> bool:
    if mass < 0.02 * diameter * diameter:
        return False
    metal_area = approximate_metal_area(section)
    if metal_area is not None and mass < 1.5 * metal_area:
        return False
    return True


def find_cpr(text: str) -> str:
    match = CPR_RE.search(text.replace("–", "-"))
    if not match:
        return ""
    value = match.group("class")
    prefix = re.match(r"[A-F](?:1|2)?ca", value, re.IGNORECASE)
    if not prefix:
        return value
    canonical = prefix.group(0)[0].upper() + prefix.group(0)[1:].lower()
    return canonical + value[prefix.end() :].replace(" ", "")


def cluster_lines(words: list[dict[str, Any]], tolerance: float = 1.8) -> list[list[dict[str, Any]]]:
    lines: list[list[dict[str, Any]]] = []
    for word in sorted(words, key=lambda item: (float(item["top"]), float(item["x0"]))):
        if not lines:
            lines.append([word])
            continue
        mean_top = sum(float(item["top"]) for item in lines[-1]) / len(lines[-1])
        if abs(float(word["top"]) - mean_top) <= tolerance:
            lines[-1].append(word)
        else:
            lines.append([word])
    for line in lines:
        line.sort(key=lambda item: float(item["x0"]))
    return lines


def line_text(words: Iterable[dict[str, Any]]) -> str:
    return compact_space(" ".join(str(word["text"]) for word in words))


def extract_heading_from_words(
    page: Any, page_words: list[dict[str, Any]]
) -> tuple[str, float]:
    words = [
        word
        for word in page_words
        if bool(word.get("upright", True))
        and float(word["top"]) < page.height * 0.30
        and 0.02 * page.width < float(word["x0"]) < 0.88 * page.width
    ]
    if not words:
        return "", 0.0
    lines = cluster_lines(words, tolerance=1.5)
    candidates: list[tuple[float, int, float, str]] = []
    for words_in_line in lines:
        text = line_text(words_in_line).strip(" ,")
        if not text or len(text) > 120:
            continue
        sizes = [float(word.get("size", 0.0)) for word in words_in_line]
        average_size = sum(sizes) / len(sizes)
        top = min(float(word["top"]) for word in words_in_line)
        cable_code_hint = int(
            any(character.isdigit() for character in text)
            or "-" in text
            or text.lower().startswith(("bit ", "bit", "cobi"))
        )
        candidates.append((average_size, cable_code_hint, -top, text))
    if not candidates:
        return "", 0.0
    best = max(candidates)
    return best[3], best[0]


def usable_heading(value: str) -> bool:
    lowered = compact_space(value).casefold().strip(" ,:;")
    if not lowered or len(lowered) > 120:
        return False
    if not value[0].isalpha() or re.match(r"^\d+\s*[x×]", value):
        return False
    forbidden_exact = {
        "parametry",
        "zastosowanie",
        "informacje",
        "wymiary",
        "charakterystyka",
        "dane techniczne",
        "kable i przewody elektroenergetyczne",
        "kable telekomunikacyjne",
    }
    if lowered in forbidden_exact:
        return False
    if lowered.startswith(("pn-en ", "norma ", "standard ")):
        return False
    return any(character.isalpha() for character in value)


def extract_elpar_heading(page: Any, words: list[dict[str, Any]]) -> str:
    left_words = [
        word
        for word in words
        if float(word["x0"]) < page.width / 2.0
        and 50.0 <= float(word["top"]) <= 145.0
    ]
    candidates: list[tuple[int, float, str]] = []
    for words_in_line in cluster_lines(left_words, tolerance=1.6):
        text = line_text(words_in_line).strip(" ,:;")
        lowered = text.casefold()
        if not text or any(
            forbidden in lowered
            for forbidden in (
                "przekrój",
                "średnica",
                "masa",
                "liczba",
                "number",
                "cross-section",
                "approximate",
                "norma",
                "standard",
                "[mm",
                "kg/km",
            )
        ):
            continue
        voltage = int(bool(re.search(r"\d+(?:[.,]\d+)?/\d+\s*k?v\b", text, re.IGNORECASE)))
        code_hint = int(
            any(character.isdigit() for character in text)
            or "-" in text
            or voltage
        )
        if not any(character.isalpha() for character in text):
            continue
        top = min(float(word["top"]) for word in words_in_line)
        candidates.append((voltage * 3 + code_hint, -top, text))
    return max(candidates)[2] if candidates else ""


def extract_bitner_heading(words: list[dict[str, Any]]) -> str:
    title_words = [
        word
        for word in words
        if float(word["top"]) < 100.0
        and bool(word.get("upright", True))
        and any(character.isalpha() for character in str(word["text"]))
    ]
    if not title_words:
        return ""
    maximum_size = max(float(word.get("size", 0.0)) for word in title_words)
    prominent = [
        word
        for word in title_words
        if float(word.get("size", 0.0)) >= maximum_size - 1.0
    ]
    lines = cluster_lines(prominent, tolerance=1.8)
    if not lines:
        return ""
    return line_text(max(lines, key=lambda line: sum(float(w.get("size", 0.0)) for w in line)))


def looks_like_family_label(value: str) -> bool:
    value = compact_space(value).strip(" ,:;")
    if not value or len(value) > 70:
        return False
    if not value[0].isalpha():
        return False
    if not (any(character.isdigit() for character in value) or "-" in value):
        return False
    letters = [character for character in value if character.isalpha()]
    if not letters:
        return False
    uppercase_ratio = sum(character.isupper() for character in letters) / len(letters)
    forbidden = ("mm", "kg", "nominal", "przekrój", "approx", "conductor", "core")
    return uppercase_ratio >= 0.60 and not any(word in value.lower() for word in forbidden)


def make_record(
    *,
    source: dict[str, Any],
    page_number: int,
    family: str,
    section: str,
    diameter: float,
    mass: float | None,
    fire_load: float | None,
    cpr: str,
    catalog_code: str = "",
    parser: str,
    extra_notes: str = "",
) -> dict[str, Any]:
    normalized_family = compact_space(family).strip(" ,")
    normalized_section = compact_space(section).strip(" ,")
    designation = (
        normalized_section
        if normalized_section.casefold().startswith(
            normalized_family.casefold() + " "
        )
        else compact_space(f"{normalized_family} {normalized_section}").strip(" ,")
    )
    source_path = str(source["file"]).replace("\\", "/")
    notes = [
        "Rekord wyodrębniony automatycznie z tabeli oficjalnego katalogu; wymaga kontroli przed użyciem projektowym.",
        "Średnica przybliżona lub katalogowa; przy tolerancji/zakresie przyjęto największą wartość.",
    ]
    if extra_notes:
        notes.append(extra_notes)
    return {
        "seedKey": "",
        "manufacturer": source["manufacturer"],
        "designation": designation,
        "catalogCode": compact_space(catalog_code),
        "outerDiameterMm": round(diameter, 4),
        "massKgPerKm": None if mass is None else round(mass, 4),
        "fireLoadMjPerM": None if fire_load is None else round(fire_load, 6),
        "cprClass": cpr,
        "source": f"{source_path}#page={page_number}",
        "sourceFile": source_path,
        "sourcePage": page_number,
        "sourceUrl": source["downloadUrl"],
        "sourceDate": source["retrievedAtUtc"][:10],
        "notes": " ".join(notes),
        "verified": False,
        "extractionMethod": parser,
        "_sourceId": source["id"],
    }


def table_column_text(
    line: list[dict[str, Any]], left: float, right: float
) -> str:
    return line_text(
        word for word in line if left <= float(word["x0"]) < right
    )


def anchored_row_variants(
    section_cell: str,
    diameter_cell: str,
    mass_cell: str,
    section_suffix: str,
) -> list[tuple[str, float, float]]:
    section_tokens = re.findall(
        r"\d+(?:[x×]\d+(?:[x×][0-9.,*/+A-Za-z-]+)+)",
        section_cell,
    )
    diameter_tokens = re.findall(
        r"\d+(?:[.,]\d+)(?:[x×]\d+(?:[.,]\d+)?)?(?:\s*±\s*\d+(?:[.,]\d+))?",
        diameter_cell,
    )
    mass_tokens = NUMBER_RE.findall(mass_cell)
    if (
        len(section_tokens) > 1
        and len(section_tokens) == len(diameter_tokens) == len(mass_tokens)
    ):
        result: list[tuple[str, float, float]] = []
        for section_raw, diameter_raw, mass_raw in zip(
            section_tokens, diameter_tokens, mass_tokens
        ):
            section = normalize_section(section_raw, section_suffix)
            diameter = maximum_dimension(diameter_raw)
            mass = parse_mass(mass_raw)
            if section is not None and diameter is not None and mass is not None:
                result.append((section, diameter, mass))
        return result

    section = normalize_section(section_cell, section_suffix)
    diameter = maximum_dimension(diameter_cell)
    mass = parse_mass(mass_cell)
    if section is None or diameter is None or mass is None:
        return []
    return [(section, diameter, mass)]


def anchored_records(
    page: Any,
    source: dict[str, Any],
    page_number: int,
    rejects: list[dict[str, Any]],
    context: dict[str, Any],
) -> list[dict[str, Any]]:
    words = page.extract_words(extra_attrs=["size"])
    if not words:
        return []
    lines = cluster_lines(words)
    heading_candidate, heading_size = extract_heading_from_words(page, words)
    if source["manufacturer"] == "ELPAR":
        elpar_heading = extract_elpar_heading(page, words)
        if usable_heading(elpar_heading):
            context["lastHeading"] = elpar_heading
        elif heading_size >= 16.0 and usable_heading(heading_candidate):
            context["lastHeading"] = heading_candidate
    elif heading_size >= 16.0 and usable_heading(heading_candidate):
        context["lastHeading"] = heading_candidate
    heading = context.get("lastHeading", "")
    page_text = line_text(words)
    cpr = find_cpr(page_text)

    units = [
        word
        for word in words
        if normalized_unit(str(word["text"])) == "kg/km"
    ]
    records: list[dict[str, Any]] = []
    seen_anchors: list[tuple[float, float]] = []
    for mass_unit in units:
        mass_x = float(mass_unit["x0"])
        unit_top = float(mass_unit["top"])
        diameter_units = [
            word
            for word in words
            if normalized_unit(str(word["text"])) == "mm"
            and float(word["x0"]) < mass_x
            and abs(float(word["top"]) - unit_top) <= 35
        ]
        diameter_units = [
            word
            for word in diameter_units
            if any(
                keyword in line_text(
                    other
                    for other in words
                    if unit_top - 95 <= float(other["top"]) < unit_top
                    and float(word["x0"]) - 45
                    <= float(other["x0"])
                    < mass_x - 2
                ).casefold()
                for keyword in ("średnic", "diameter", "wymiar")
            )
        ]
        section_units: list[dict[str, Any]] = []
        for word in words:
            unit_name = normalized_unit(str(word["text"]))
            raw_unit = compact_space(str(word["text"]))
            split_square_unit = (
                raw_unit.lower().startswith("[mm")
                and not raw_unit.endswith("]")
                and any(
                    compact_space(str(other["text"])).strip("[]") == "2"
                    and 0 < float(other["x0"]) - float(word["x0"]) < 35
                    and abs(float(other["top"]) - float(word["top"])) < 12
                    for other in words
                )
            )
            split_count_square_unit = (
                raw_unit.lower().startswith("[n")
                and any(
                    normalized_unit(str(other["text"])) == "mm"
                    and 0 < float(other["x0"]) - float(word["x0"]) < 45
                    and abs(float(other["top"]) - float(word["top"])) < 5
                    for other in words
                )
                and any(
                    compact_space(str(other["text"])).strip("[]") == "2"
                    and 0 < float(other["x0"]) - float(word["x0"]) < 55
                    and abs(float(other["top"]) - float(word["top"])) < 12
                    for other in words
                )
            )
            if (
                unit_name == "mm2"
                or split_square_unit
                or split_count_square_unit
                or (
                    unit_name.endswith("mm")
                    and "x" in unit_name
                    and "n" in unit_name
                )
            ) and float(word["x0"]) < mass_x and abs(
                float(word["top"]) - unit_top
            ) <= 45:
                section_units.append(word)
        if not diameter_units or not section_units:
            continue
        diameter_unit = max(
            diameter_units,
            key=lambda word: (
                round(float(word["x0"]), 1),
                -abs(float(word["top"]) - unit_top),
            ),
        )
        diameter_x = float(diameter_unit["x0"])
        eligible_sections = [
            word for word in section_units if float(word["x0"]) < diameter_x
        ]
        if not eligible_sections:
            continue
        section_unit = max(
            eligible_sections,
            key=lambda word: (
                round(float(word["x0"]), 1),
                -abs(float(word["top"]) - unit_top),
            ),
        )
        section_x = float(section_unit["x0"])
        section_unit_name = normalized_unit(str(section_unit["text"]))
        section_unit_raw = compact_space(str(section_unit["text"]))
        section_suffix = (
            "mm²"
            if section_unit_name.endswith("mm2")
            or (
                section_unit_raw.lower().startswith("[mm")
                and not section_unit_raw.endswith("]")
            )
            or section_unit_raw.lower().startswith("[n")
            else "mm"
        )
        if not (section_x < diameter_x < mass_x):
            continue
        anchor = (round(section_x, 1), round(mass_x, 1))
        if any(abs(anchor[0] - old[0]) < 3 and abs(anchor[1] - old[1]) < 3 for old in seen_anchors):
            continue
        seen_anchors.append(anchor)

        half_right = (
            page.width / 2.0
            if mass_x < page.width / 2.0
            else page.width
        )
        section_fragment_columns: set[float] = set()
        if section_unit_raw.lower().startswith("[n"):
            section_fragment_columns = {
                float(word["x0"])
                for word in words
                if section_x < float(word["x0"]) < section_x + 60.0
                and abs(float(word["top"]) - float(section_unit["top"])) < 6.0
                and compact_space(str(word["text"])).strip("[]").casefold()
                in {"x", "mm", "2"}
            }
        header_columns = sorted(
            {
                float(word["x0"])
                for word in words
                if abs(float(word["top"]) - unit_top) <= 30
                and normalized_unit(str(word["text"]))
                in {
                    "mm",
                    "mm2",
                    "kg/km",
                    "ω/km",
                    "ωxkm",
                    "kwh/m",
                    "mj/m",
                    "cpr",
                }
                and not any(
                    abs(float(word["x0"]) - fragment_x) < 2.0
                    for fragment_x in section_fragment_columns
                )
                and (
                    0 <= float(word["x0"]) < page.width / 2.0
                    if mass_x < page.width / 2.0
                    else page.width / 2.0 <= float(word["x0"]) < page.width
                )
            }
            | {section_x, diameter_x, mass_x}
        )

        def previous_column(
            x: float, fallback: float, minimum_x: float = 0.0
        ) -> float:
            previous = [
                column
                for column in header_columns
                if minimum_x <= column < x - 2
            ]
            return max(previous) if previous else fallback

        def next_column(x: float, fallback: float) -> float:
            following = [column for column in header_columns if column > x + 2]
            return min(following) if following else fallback

        # Many two-page spreads center the unit header while left-aligning the
        # actual conductor designation 10–15 points earlier.
        section_left = max(0.0, section_x - 24.0)
        section_right = next_column(section_x, diameter_x) - 5.0
        diameter_left = diameter_x - 5.0
        diameter_right = next_column(diameter_x, mass_x) - 5.0
        mass_left = mass_x - 5.0
        mass_right = next_column(mass_x, half_right) - 5.0
        energy_units = [
            word
            for word in words
            if normalized_unit(str(word["text"])) in {"kwh/m", "mj/m"}
            and mass_x < float(word["x0"]) < half_right
            and abs(float(word["top"]) - unit_top) <= 25
        ]
        energy_unit = (
            min(energy_units, key=lambda word: float(word["x0"]))
            if energy_units
            else None
        )
        energy_x = float(energy_unit["x0"]) if energy_unit else 0.0
        energy_left = (
            energy_x - 5.0
            if energy_unit
            else 0.0
        )
        energy_right = (
            next_column(energy_x, half_right) - 5.0
            if energy_unit
            else 0.0
        )
        current_family = ""
        last_record_top: float | None = None
        for words_in_line in lines:
            top = min(float(word["top"]) for word in words_in_line)
            if top <= unit_top + 2:
                continue
            if last_record_top is not None and top - last_record_top > 36.0:
                break
            section_cell = table_column_text(words_in_line, section_left, section_right)
            diameter_cell = table_column_text(words_in_line, diameter_left, diameter_right)
            mass_cell = table_column_text(words_in_line, mass_left, mass_right)
            energy_cell = (
                table_column_text(words_in_line, energy_left, energy_right)
                if energy_unit
                else ""
            )
            variants = anchored_row_variants(
                section_cell, diameter_cell, mass_cell, section_suffix
            )
            valid_variants = [
                variant
                for variant in variants
                if 0.5 <= variant[1] <= 250.0
                and 0.1 <= variant[2] <= 100000.0
            ]

            if valid_variants:
                family = current_family or heading
                if not family:
                    rejects.append(
                        {
                            "sourceId": source["id"],
                            "page": page_number,
                            "reason": "missing-family-heading",
                            "raw": line_text(words_in_line),
                        }
                    )
                    continue
                for section, diameter, mass in valid_variants:
                    records.append(
                        make_record(
                            source=source,
                            page_number=page_number,
                            family=family,
                            section=section,
                            diameter=diameter,
                            mass=mass,
                            fire_load=(
                                (
                                    maximum_dimension(energy_cell) * 3.6
                                    if normalized_unit(str(energy_unit["text"]))
                                    == "kwh/m"
                                    else maximum_dimension(energy_cell)
                                )
                                if maximum_dimension(energy_cell) is not None
                                and len(valid_variants) == 1
                                else None
                            ),
                            cpr=cpr,
                            parser="pdf-anchored-columns-v1",
                            extra_notes=(
                                "Ciepło spalania podane przez producenta w kWh/m "
                                "przeliczono na MJ/m współczynnikiem 3,6."
                                if energy_unit
                                and maximum_dimension(energy_cell) is not None
                                and len(valid_variants) == 1
                                else ""
                            ),
                        )
                    )
                last_record_top = top
                continue

            if (
                looks_like_family_label(section_cell)
                and not variants
                and maximum_dimension(diameter_cell) is None
                and parse_mass(mass_cell) is None
            ):
                current_family = compact_space(section_cell).strip(" ,:;")
            elif (
                normalize_section(section_cell, section_suffix) is not None
                and maximum_dimension(diameter_cell) is not None
                and parse_mass(mass_cell) is None
                and any(character.isdigit() for character in mass_cell)
            ):
                rejects.append(
                    {
                        "sourceId": source["id"],
                        "page": page_number,
                        "reason": "unparsed-mass",
                        "raw": line_text(words_in_line),
                    }
                )
    return records


def header_index(headers: list[str], patterns: tuple[str, ...]) -> int | None:
    for index, header in enumerate(headers):
        lowered = compact_space(header).lower()
        if any(pattern in lowered for pattern in patterns):
            return index
    return None


def clean_catalog_code(value: str) -> str:
    value = compact_space(value).replace(" ", "")
    return value.strip(" ,;")


def bitner_records(
    page: Any,
    source: dict[str, Any],
    page_number: int,
    rejects: list[dict[str, Any]],
    context: dict[str, Any],
) -> list[dict[str, Any]]:
    del context
    words = []
    for original in page.extract_words(extra_attrs=["size"]):
        decoded = dict(original)
        decoded["text"] = decode_bitner_cid(str(original["text"]))
        words.append(decoded)
    heading = BITNER_HEADING_OVERRIDES.get(
        page_number,
        extract_bitner_heading(words),
    )
    page_text = line_text(words)
    cpr = find_cpr(page_text)
    records: list[dict[str, Any]] = []
    for table in page.extract_tables():
        if not table:
            continue
        table = [
            [
                decode_bitner_cid(cell) if cell is not None else None
                for cell in row
            ]
            for row in table
        ]
        header_row_index = None
        columns: tuple[int, int, int, int] | None = None
        for candidate_index, candidate in enumerate(table[:5]):
            headers = [compact_space(cell or "") for cell in candidate]
            code_index = header_index(headers, ("nr kat", "catalogue no", "catalog no"))
            section_index = header_index(headers, ("n x mm", "nxmm", "n×mm"))
            diameter_index = header_index(
                headers,
                ("średnic", "rednic", "diameter", "szeroko", "width"),
            )
            mass_index = header_index(headers, ("waga", "weight"))
            if None not in (code_index, section_index, diameter_index, mass_index):
                header_row_index = candidate_index
                columns = (
                    int(code_index),
                    int(section_index),
                    int(diameter_index),
                    int(mass_index),
                )
                break
            if (
                len(headers) >= 4
                and "nr kat" in headers[0].lower()
                and ("n x mm" in headers[1].lower() or "nxmm" in headers[1].lower())
            ):
                header_row_index = candidate_index
                columns = (0, 1, 2, 3)
                break
        if header_row_index is None or columns is None:
            continue
        code_index, section_index, diameter_index, mass_index = columns
        preview = table[header_row_index + 1 : header_row_index + 8]

        def has_data(column: int) -> bool:
            return any(
                len(row) > column
                and compact_space(row[column] or "")
                and any(character.isdigit() for character in compact_space(row[column] or ""))
                for row in preview
            )

        def has_catalog_code(column: int) -> bool:
            return any(
                len(row) > column
                and any(
                    CATALOG_CODE_RE.fullmatch(clean_catalog_code(token))
                    for token in re.split(
                        r"\s+",
                        compact_space(row[column] or ""),
                    )
                    if token
                )
                for row in preview
            )

        if not has_catalog_code(code_index):
            for candidate in (code_index - 1, code_index + 1):
                if candidate >= 0 and has_catalog_code(candidate):
                    code_index = candidate
                    break
        if section_index <= code_index or not has_data(section_index):
            for candidate in (section_index - 1, section_index + 1):
                if candidate > code_index and has_data(candidate):
                    section_index = candidate
                    break
        if diameter_index <= section_index or not has_data(diameter_index):
            for candidate in (diameter_index - 1, diameter_index + 1):
                if candidate > section_index and has_data(candidate):
                    diameter_index = candidate
                    break
        if mass_index <= diameter_index or not has_data(mass_index):
            for candidate in (mass_index - 1, mass_index + 1):
                if candidate > diameter_index and has_data(candidate):
                    mass_index = candidate
                    break
        columns = (code_index, section_index, diameter_index, mass_index)
        maximum_index = max(columns)
        for row in table[header_row_index + 1 :]:
            if len(row) <= maximum_index:
                continue
            code_raw = compact_space(row[code_index] or "")
            section_raw = compact_space(row[section_index] or "")
            diameter_raw = compact_space(row[diameter_index] or "")
            mass_raw = compact_space(row[mass_index] or "")
            if not code_raw or not any(character.isdigit() for character in code_raw):
                continue
            codes = [
                clean_catalog_code(token)
                for token in re.split(r"\s+", code_raw)
                if CATALOG_CODE_RE.fullmatch(clean_catalog_code(token))
                and clean_catalog_code(token) == clean_catalog_code(token).upper()
                and any(character.isdigit() for character in clean_catalog_code(token))
            ]
            if not codes:
                # A small number of PDF cells overlay an alphanumeric code on
                # header text (for example IP4037 on "cable weight"). The code
                # remains recoverable from its uppercase letters and digits.
                recovered_code = "".join(
                    character for character in code_raw if character.isupper()
                ) + "".join(character for character in code_raw if character.isdigit())
                if re.fullmatch(r"[A-Z]{1,4}\d{3,6}", recovered_code):
                    codes = [recovered_code]
            variants: list[tuple[str, str, float, float]] = []
            if len(codes) > 1:
                sections = [
                    section
                    for token in re.split(r"\s+", section_raw)
                    if (section := normalize_section(token)) is not None
                ]
                diameters = parsed_numbers(diameter_raw)
                masses = parsed_numbers(mass_raw)
                if len(codes) == len(sections) == len(diameters) == len(masses):
                    variants = list(zip(codes, sections, diameters, masses))
            elif len(codes) == 1:
                section = normalize_section(section_raw)
                diameter = maximum_dimension(diameter_raw)
                mass = parse_mass(mass_raw)
                if mass is None:
                    for candidate in (mass_index - 1, mass_index + 1):
                        if candidate > diameter_index and candidate < len(row):
                            mass = parse_mass(compact_space(row[candidate] or ""))
                            if mass is not None:
                                break
                if section is not None and diameter is not None and mass is not None:
                    variants = [(codes[0], section, diameter, mass)]

            valid_variants = [
                (code, section, diameter, mass)
                for code, section, diameter, mass in variants
                if 0.5 <= diameter <= 250.0
                and 0.1 <= mass <= 100000.0
                and plausible_cable_dimensions(section, diameter, mass)
            ]
            if not valid_variants:
                rejects.append(
                    {
                        "sourceId": source["id"],
                        "page": page_number,
                        "reason": "invalid-bitner-table-row",
                        "raw": " | ".join(compact_space(cell or "") for cell in row),
                    }
                )
                continue
            if not heading:
                rejects.append(
                    {
                        "sourceId": source["id"],
                        "page": page_number,
                        "reason": "missing-family-heading",
                        "raw": " | ".join(compact_space(cell or "") for cell in row),
                    }
                )
                continue
            for code, section, diameter, mass in valid_variants:
                records.append(
                    make_record(
                        source=source,
                        page_number=page_number,
                        family=heading,
                        section=section,
                        diameter=diameter,
                        mass=mass,
                        fire_load=None,
                        cpr=cpr,
                        catalog_code=code,
                        parser="pdf-grid-table-v1",
                    )
                )
    return records


def cobi_records(
    page: Any,
    source: dict[str, Any],
    page_number: int,
    rejects: list[dict[str, Any]],
    context: dict[str, Any],
) -> list[dict[str, Any]]:
    del rejects, context
    text = page.extract_text(layout=True) or ""
    records: list[dict[str, Any]] = []
    preceding_product_name = ""
    for raw_line in text.splitlines():
        code_matches = list(COBI_CODE_RE.finditer(raw_line))
        compact_line = compact_space(raw_line)
        if (
            not code_matches
            and re.search(r"\b(cable|kabel|przewód|cord)\b", compact_line, re.IGNORECASE)
            and len(compact_line) <= 180
        ):
            preceding_product_name = compact_line
        for index, code_match in enumerate(code_matches):
            end = code_matches[index + 1].start() if index + 1 < len(code_matches) else len(raw_line)
            segment = compact_space(raw_line[code_match.start() : end])
            dimension_match = re.search(
                r"(?P<base>\d{1,3}[.,]\d+)"
                r"(?:\s*±\s*(?P<tolerance>\d{1,2}[.,]\d+))?\s*mm\b",
                segment,
                re.IGNORECASE,
            )
            if not dimension_match:
                continue
            before_dimension = segment[len(code_match.group(0)) : dimension_match.start()].strip()
            cpr_matches = list(CPR_RE.finditer(before_dimension))
            cpr = find_cpr(before_dimension)
            cut_positions = [match.start() for match in cpr_matches]
            percent = re.search(r"\b\d{1,3}\s*%", before_dimension)
            gauge = re.search(r"\b\d{1,2}\s*AWG\b", before_dimension, re.IGNORECASE)
            if percent:
                cut_positions.append(percent.start())
            if gauge:
                cut_positions.append(gauge.start())
            name_end = min(cut_positions) if cut_positions else len(before_dimension)
            name = compact_space(before_dimension[:name_end]).strip(" ,-")
            if not re.search(r"\b(cable|kabel|przewód|cord)\b", name, re.IGNORECASE):
                name = preceding_product_name
            if not name:
                continue
            base = float(dimension_match.group("base").replace(",", "."))
            tolerance = (
                float(dimension_match.group("tolerance").replace(",", "."))
                if dimension_match.group("tolerance")
                else 0.0
            )
            diameter = base + tolerance
            records.append(
                make_record(
                    source=source,
                    page_number=page_number,
                    family=name,
                    section="",
                    diameter=diameter,
                    mass=None,
                    fire_load=None,
                    cpr=cpr,
                    catalog_code=code_match.group(0).upper(),
                    parser="pdf-layout-row-v1",
                    extra_notes=(
                        f"Wartość nominalna {format_decimal(base)} mm"
                        + (
                            f" z tolerancją ±{format_decimal(tolerance)} mm; do obliczeń użyto maksimum."
                            if tolerance
                            else "."
                        )
                    ),
                )
            )
    return records


def record_quality(record: dict[str, Any], priority: int) -> tuple[int, int, int]:
    return (
        priority,
        int(record["massKgPerKm"] is not None),
        int(bool(record["cprClass"])),
    )


def deduplicate(
    records: list[dict[str, Any]], priorities: dict[str, int]
) -> tuple[list[dict[str, Any]], int]:
    chosen: dict[tuple[str, str], dict[str, Any]] = {}
    duplicate_count = 0
    for record in records:
        manufacturer = compact_space(record["manufacturer"]).casefold()
        catalog_code = compact_space(record["catalogCode"]).casefold()
        identity = catalog_code or compact_space(record["designation"]).casefold()
        key = (manufacturer, identity)
        existing = chosen.get(key)
        if existing is None:
            chosen[key] = record
            continue
        duplicate_count += 1
        existing_quality = record_quality(
            existing, priorities.get(existing["_sourceId"], 0)
        )
        new_quality = record_quality(record, priorities.get(record["_sourceId"], 0))
        if new_quality > existing_quality:
            chosen[key] = record

    result = list(chosen.values())
    for record in result:
        identity = record["catalogCode"] or record["designation"]
        digest = hashlib.sha256(
            f'{record["manufacturer"]}|{identity}'.encode("utf-8")
        ).hexdigest()[:24]
        record["seedKey"] = f"official:{digest}"
    result.sort(
        key=lambda record: (
            str(record["manufacturer"]).casefold(),
            str(record["designation"]).casefold(),
            str(record["catalogCode"]).casefold(),
        )
    )
    return result, duplicate_count


def validate_records(records: list[dict[str, Any]]) -> None:
    seed_keys: set[str] = set()
    for record in records:
        if not record["manufacturer"] or not record["designation"]:
            raise RuntimeError(f"Empty identity in record: {record}")
        diameter = record["outerDiameterMm"]
        if not isinstance(diameter, (int, float)) or not 0.5 <= diameter <= 250:
            raise RuntimeError(f"Invalid diameter in record: {record}")
        mass = record["massKgPerKm"]
        if mass is not None and (
            not isinstance(mass, (int, float)) or not 0.1 <= mass <= 100000
        ):
            raise RuntimeError(f"Invalid mass in record: {record}")
        fire_load = record["fireLoadMjPerM"]
        if fire_load is not None and (
            not isinstance(fire_load, (int, float)) or not 0.0 <= fire_load <= 100000
        ):
            raise RuntimeError(f"Invalid fire load in record: {record}")
        if record["seedKey"] in seed_keys:
            raise RuntimeError(f'Duplicate seed key: {record["seedKey"]}')
        seed_keys.add(record["seedKey"])


def assert_control_samples(records: list[dict[str, Any]]) -> None:
    by_code = {
        (record["manufacturer"], record["catalogCode"]): record
        for record in records
        if record["catalogCode"]
    }
    bitner = by_code.get(("BITNER", "S30001"))
    if not bitner or bitner["outerDiameterMm"] != 3.4 or bitner["massKgPerKm"] != 16:
        raise RuntimeError("Control sample BITNER S30001 was not extracted correctly.")
    bitner_layout_samples = {
        "B50900": (11.4, 120),
        "BS1900": (24.7, 815),
        "IP4037": (41.0, 2925),
        "IP4090": (16.2, 169),
        "LP0185": (7.1, 62),
        "B63877": (6.0, 53),
    }
    for code, (diameter, mass) in bitner_layout_samples.items():
        record = by_code.get(("BITNER", code))
        if (
            not record
            or record["outerDiameterMm"] != diameter
            or record["massKgPerKm"] != mass
        ):
            raise RuntimeError(
                f"Control sample BITNER {code} was not extracted correctly."
            )
    cobi = by_code.get(("CobiCabling", "C23-2511"))
    if not cobi or cobi["outerDiameterMm"] != 8.7 or cobi["massKgPerKm"] is not None:
        raise RuntimeError("Control sample CobiCabling C23-2511 was not extracted correctly.")
    tfk = [
        record
        for record in records
        if record["manufacturer"] == "TELE-FONIKA Kable"
        and "H05V-U" in record["designation"]
        and "0,5 mm²" in record["designation"]
    ]
    if not any(
        record["outerDiameterMm"] == 2.0 and record["massKgPerKm"] == 8
        for record in tfk
    ):
        raise RuntimeError("Control sample TELE-FONIKA H05V-U 0.5 mm² was not extracted.")
    elpar = [
        record
        for record in records
        if record["manufacturer"] == "ELPAR"
        and "AALXS" in record["designation"]
        and "25 mm²" in record["designation"]
    ]
    if not any(
        record["outerDiameterMm"] == 10.8 and record["massKgPerKm"] == 126.2
        for record in elpar
    ):
        raise RuntimeError("Control sample ELPAR AALXS 25 mm² was not extracted.")


def parse_source(
    root: Path,
    source: dict[str, Any],
    spec: SourceSpec,
    rejects: list[dict[str, Any]],
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    report = {
        "sourceId": source["id"],
        "file": source["file"],
        "role": "primary" if spec.primary else "supplemental",
        "parser": spec.parser,
        "pagesScanned": 0,
        "rawRecords": 0,
        "note": "",
    }
    if not spec.primary:
        report["note"] = (
            "Materiał pomocniczy: służy do kontroli kontekstu/CPR, "
            "nie tworzy samodzielnych wariantów bez średnicy i masy."
        )
        return [], report

    pdf_path = root / source["file"]
    records: list[dict[str, Any]] = []
    context: dict[str, Any] = {}
    parser = {
        "anchored": anchored_records,
        "bitner": bitner_records,
        "cobi": cobi_records,
    }[spec.parser]
    with pdfplumber.open(pdf_path) as pdf:
        for page_index, page in enumerate(pdf.pages):
            page_number = page_index + 1
            try:
                page_records = parser(page, source, page_number, rejects, context)
                records.extend(page_records)
            except Exception as error:  # keep an auditable reject instead of hiding a page
                rejects.append(
                    {
                        "sourceId": source["id"],
                        "page": page_number,
                        "reason": "page-parser-error",
                        "raw": f"{type(error).__name__}: {error}",
                    }
                )
            finally:
                close_page = getattr(page, "close", None)
                if close_page:
                    close_page()
            report["pagesScanned"] = page_number
            if page_number % 10 == 0 or page_number == len(pdf.pages):
                print(
                    f'[{source["id"]}] {page_number}/{len(pdf.pages)} pages, '
                    f"{len(records)} raw records",
                    flush=True,
                )
    report["rawRecords"] = len(records)
    return records, report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("resources/catalog-seed.json"),
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=Path("source-materials/extraction-report.json"),
    )
    parser.add_argument(
        "--rejects",
        type=Path,
        default=Path("source-materials/extraction-rejects.json"),
    )
    parser.add_argument(
        "--log",
        type=Path,
        help="Optional UTF-8 progress log (useful for unattended generation).",
    )
    parser.add_argument(
        "--only-source",
        choices=[spec.source_id for spec in SOURCE_SPECS],
        help="Extract only one source (development/diagnostics)",
    )
    args = parser.parse_args()
    root = args.root.resolve()
    log_file = None
    if args.log:
        log_path = root / args.log
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_file = log_path.open("w", encoding="utf-8")
        sys.stdout = log_file
        sys.stderr = log_file
    manifest = json.loads((root / "source-materials/manifest.json").read_text(encoding="utf-8-sig"))
    sources = {source["id"]: source for source in manifest["sources"]}
    priorities = {spec.source_id: spec.priority for spec in SOURCE_SPECS}

    all_records: list[dict[str, Any]] = []
    rejects: list[dict[str, Any]] = []
    source_reports: list[dict[str, Any]] = []
    for spec in SOURCE_SPECS:
        if args.only_source and spec.source_id != args.only_source:
            continue
        source = sources[spec.source_id]
        print(f'Opening {spec.source_id}: {source["file"]}', flush=True)
        records, report = parse_source(root, source, spec, rejects)
        all_records.extend(records)
        source_reports.append(report)

    records, duplicate_count = deduplicate(all_records, priorities)
    validate_records(records)
    if not args.only_source:
        assert_control_samples(records)

    by_manufacturer: dict[str, int] = defaultdict(int)
    missing_mass = 0
    missing_fire_load = 0
    for record in records:
        by_manufacturer[record["manufacturer"]] += 1
        missing_mass += int(record["massKgPerKm"] is None)
        missing_fire_load += int(record["fireLoadMjPerM"] is None)

    public_records = []
    for record in records:
        record = dict(record)
        record.pop("_sourceId", None)
        public_records.append(record)

    output_document = {
        "schemaVersion": 2,
        "datasetVersion": DATASET_VERSION,
        "generatedOn": date.today().isoformat(),
        "notice": (
            "Dane wyodrębniono automatycznie z oficjalnych katalogów zapisanych w "
            "source-materials. Każdy rekord wymaga weryfikacji przed użyciem projektowym. "
            "Brak masy lub MJ/m pozostaje brakiem danych; CPR nie jest przeliczane na MJ/m."
        ),
        "items": public_records,
    }
    report_document = {
        "schemaVersion": 1,
        "datasetVersion": DATASET_VERSION,
        "generatedOn": date.today().isoformat(),
        "sourceManifest": "source-materials/manifest.json",
        "summary": {
            "records": len(public_records),
            "rawRecords": len(all_records),
            "duplicatesRemoved": duplicate_count,
            "rejectedRows": len(rejects),
            "missingMassRecords": missing_mass,
            "missingFireLoadRecords": missing_fire_load,
            "byManufacturer": dict(sorted(by_manufacturer.items())),
        },
        "sources": source_reports,
        "controlSamples": [
            "TELE-FONIKA H05V-U 1 x 0,5 mm²: 2,0 mm / 8 kg/km",
            "ELPAR AALXS 1 x 25 mm²: 10,8 mm / 126,2 kg/km",
            "BITNER S30001: 3,4 mm / 16 kg/km",
            "BITNER B50900: maks. wymiar 11,4 mm / 120 kg/km",
            "BITNER BS1900: 24,7 mm / 815 kg/km (wiersz wielowariantowy)",
            "BITNER IP4037: maks. 41,0 mm / 2925 kg/km (kod nałożony na nagłówek PDF)",
            "BITNER IP4090: szerokość 16,2 mm / 169 kg/km",
            "BITNER LP0185: 7,1 mm / 62 kg/km (masa przesunięta w siatce PDF)",
            "BITNER B63877: 6,0 mm / 53 kg/km (przesunięta kolumna kodu)",
            "CobiCabling C23-2511: maks. 8,7 mm / brak masy",
        ],
        "limitations": [
            "Automatyczne rekordy mają verified=false do czasu ręcznej kontroli.",
            "Wartości średnic opisane zakresem lub tolerancją są zapisywane jako maksimum.",
            "Ciepło spalania dostępne dla części wariantów TELE-FONIKA w kWh/m przeliczono jednostkowo przez 3,6 na MJ/m; żadnej wartości nie wyprowadzono z CPR.",
            "Materiały pomocnicze nie tworzą duplikatów wariantów z katalogów głównych.",
        ],
    }

    output_path = root / args.output
    report_path = root / args.report
    rejects_path = root / args.rejects
    output_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    rejects_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(output_document, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    report_path.write_text(
        json.dumps(report_document, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    rejects_path.write_text(
        json.dumps(
            {
                "schemaVersion": 1,
                "datasetVersion": DATASET_VERSION,
                "count": len(rejects),
                "items": rejects,
            },
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(json.dumps(report_document["summary"], ensure_ascii=False, indent=2))
    if log_file:
        log_file.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
