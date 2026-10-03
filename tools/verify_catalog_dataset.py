#!/usr/bin/env python3
"""Fast consistency checks for the generated in-application cable catalog."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any


MINIMUM_COUNTS = {
    "TELE-FONIKA Kable": 2000,
    "ELPAR": 5000,
    "BITNER": 8000,
    "CobiCabling": 20,
}


def fail(message: str) -> None:
    raise RuntimeError(message)


def load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root",
    )
    args = parser.parse_args()
    root = args.root.resolve()

    dataset = load_json(root / "resources/catalog-seed.json")
    report = load_json(root / "source-materials/extraction-report.json")
    rejects = load_json(root / "source-materials/extraction-rejects.json")
    manifest = load_json(root / "source-materials/manifest.json")
    items = dataset.get("items", [])

    if not dataset.get("datasetVersion"):
        fail("Missing datasetVersion.")
    if dataset["datasetVersion"] != report.get("datasetVersion"):
        fail("Dataset and extraction report versions differ.")
    if len(items) != report.get("summary", {}).get("records"):
        fail("Dataset record count differs from extraction report.")
    if rejects.get("count") != report.get("summary", {}).get("rejectedRows"):
        fail("Reject count differs from extraction report.")
    expected_rejected_codes = {"H63070", "EM9076", "S70282", "S70353"}
    rejected_codes = {
        item.get("raw", "").split("|", maxsplit=1)[0].strip()
        for item in rejects.get("items", [])
    }
    if rejected_codes != expected_rejected_codes:
        fail(f"Unexpected rejected rows: {sorted(rejected_codes)}")

    manifest_sources = {entry["file"] for entry in manifest["sources"]}
    counts = Counter()
    seed_keys: set[str] = set()
    direct_fire_loads = 0
    missing_mass = 0
    missing_fire_load = 0
    required = {
        "seedKey",
        "manufacturer",
        "designation",
        "outerDiameterMm",
        "massKgPerKm",
        "fireLoadMjPerM",
        "sourceFile",
        "sourcePage",
        "sourceUrl",
        "extractionMethod",
    }

    for index, item in enumerate(items):
        absent = required - item.keys()
        if absent:
            fail(f"Record {index} lacks fields: {sorted(absent)}")
        if item["seedKey"] in seed_keys:
            fail(f"Duplicate seed key: {item['seedKey']}")
        seed_keys.add(item["seedKey"])
        if item["sourceFile"] not in manifest_sources:
            fail(f"Unknown source file: {item['sourceFile']}")
        if not (root / item["sourceFile"]).is_file():
            fail(f"Archived source is missing: {item['sourceFile']}")
        if not isinstance(item["sourcePage"], int) or item["sourcePage"] <= 0:
            fail(f"Invalid source page in record {index}.")
        diameter = item["outerDiameterMm"]
        if not isinstance(diameter, (int, float)) or not 0.5 <= diameter <= 250:
            fail(f"Invalid diameter in record {index}.")
        mass = item["massKgPerKm"]
        if mass is None:
            missing_mass += 1
        elif not isinstance(mass, (int, float)) or mass <= 0:
            fail(f"Invalid mass in record {index}.")
        fire_load = item["fireLoadMjPerM"]
        if fire_load is None:
            missing_fire_load += 1
        elif not isinstance(fire_load, (int, float)) or fire_load <= 0:
            fail(f"Invalid fire load in record {index}.")
        else:
            direct_fire_loads += 1
            if item["manufacturer"] != "TELE-FONIKA Kable":
                fail("Only directly catalogued TELE-FONIKA heat values are expected.")
        counts[item["manufacturer"]] += 1

    for manufacturer, minimum in MINIMUM_COUNTS.items():
        if counts[manufacturer] < minimum:
            fail(
                f"{manufacturer}: expected at least {minimum}, "
                f"found {counts[manufacturer]}."
            )
    if direct_fire_loads == 0:
        fail("No direct heat-of-combustion values were retained.")
    malformed_bitner_names = [
        item["designation"]
        for item in items
        if item["manufacturer"] == "BITNER"
        and (
            "?" in item["designation"]
            or item["designation"].startswith("Nr kat.")
        )
    ]
    if malformed_bitner_names:
        fail("BITNER designations still contain undecoded or generic headings.")

    by_code = {
        (item["manufacturer"], item.get("catalogCode", "")): item
        for item in items
        if item.get("catalogCode")
    }
    controls = {
        ("BITNER", "S30001"): (3.4, 16),
        ("BITNER", "B50900"): (11.4, 120),
        ("BITNER", "BS1900"): (24.7, 815),
        ("BITNER", "IP4037"): (41.0, 2925),
        ("BITNER", "IP4090"): (16.2, 169),
        ("BITNER", "LP0185"): (7.1, 62),
        ("BITNER", "B63877"): (6.0, 53),
        ("CobiCabling", "C23-2511"): (8.7, None),
    }
    for identity, (diameter, mass) in controls.items():
        item = by_code.get(identity)
        if (
            not item
            or item["outerDiameterMm"] != diameter
            or item["massKgPerKm"] != mass
        ):
            fail(f"Control sample is invalid or missing: {identity}")
    tfk_energy_sample = any(
        item["manufacturer"] == "TELE-FONIKA Kable"
        and item["sourcePage"] == 81
        and item["outerDiameterMm"] == 5.4
        and item["massKgPerKm"] == 44
        and item["fireLoadMjPerM"] == 0.828
        for item in items
    )
    if not tfk_energy_sample:
        fail("TELE-FONIKA 0.23 kWh/m -> 0.828 MJ/m control sample is missing.")

    expected_summary = report["summary"]
    if missing_mass != expected_summary.get("missingMassRecords"):
        fail("Missing-mass count differs from extraction report.")
    if missing_fire_load != expected_summary.get("missingFireLoadRecords"):
        fail("Missing-fire-load count differs from extraction report.")

    print(
        json.dumps(
            {
                "datasetVersion": dataset["datasetVersion"],
                "records": len(items),
                "byManufacturer": dict(sorted(counts.items())),
                "missingMassRecords": missing_mass,
                "missingFireLoadRecords": missing_fire_load,
                "directFireLoadRecords": direct_fire_loads,
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
