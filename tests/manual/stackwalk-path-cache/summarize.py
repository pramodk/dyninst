#!/usr/bin/env python3
import csv
import pathlib
import sys
import xml.etree.ElementTree as ET


def malloc_totals(path):
    root = ET.parse(path).getroot()
    system = root.find("./system[@type='current']")
    mmap = root.find("./total[@type='mmap']")
    fast = root.find("./total[@type='fast']")
    rest = root.find("./total[@type='rest']")
    arena = int(system.attrib["size"])
    mapped = int(mmap.attrib["size"])
    free = int(fast.attrib["size"]) + int(rest.attrib["size"])
    return arena, free, mapped, arena - free + mapped


for result in map(pathlib.Path, sys.argv[1:]):
    print(result)
    with (result / "stages.tsv").open() as stream:
        rows = [row for row in csv.DictReader(stream, delimiter="\t")
                if row.get("stage") and row["stage"] != "summary"]
    selected = [row for row in rows if row["stage"] in
                {"before_attach", "after_libraries", "after_detach", "after_delete"}]
    print("stage\tindex\telapsed_s\trss_mib\tanon_mib\tprivate_dirty_mib\t"
          "arena_mib\tmalloc_in_use_mib\tframes\tnamed\tlibrary_names")
    for row in selected:
        arena, _, _, used = malloc_totals(row["malloc_info"])
        mib = 1024 * 1024
        print("\t".join((
            row["stage"], row["index"], f'{float(row["elapsed_s"]):.3f}',
            f'{int(row["rss_kib"]) / 1024:.1f}',
            f'{int(row["anon_kib"]) / 1024:.1f}',
            f'{int(row["private_dirty_kib"]) / 1024:.1f}',
            f'{arena / mib:.1f}', f'{used / mib:.1f}', row["frames"],
            row["named"], row["library_names"])))
    print()
