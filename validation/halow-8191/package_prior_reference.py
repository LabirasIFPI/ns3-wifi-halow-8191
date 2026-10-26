#!/usr/bin/env python3
"""Package the incomplete 2026-10-13 8,191-STA reference for audit."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import shutil
import zipfile


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def read_csv(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or any(None in row or None in row.values() for row in rows):
        raise ValueError(f"Malformed or empty CSV: {path}")
    return rows


def write_csv(path, rows, fields):
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    run = args.run.resolve()
    output = args.output.resolve()
    archive = output.with_suffix(".zip")
    if output.exists() or archive.exists():
        raise ValueError("Output already exists; choose a new destination")

    execution = json.loads((run / "execution.json").read_text())
    if execution.get("status") != "finished" or execution.get("exit_code") != 0:
        raise ValueError("The source execution did not finish successfully")
    if execution["parameters"]["stations"] != 8191:
        raise ValueError("This package is specific to the 8,191-STA reference")

    summaries = read_csv(run / "run-summary.csv")
    overall_rows = read_csv(run / "run-run.csv")
    if len(summaries) != 8191 or len(overall_rows) != 1:
        raise ValueError("Unexpected source row count")
    aids = {int(row["aid"]) for row in summaries}
    if aids != set(range(1, 8192)):
        raise ValueError("AIDs do not cover 1..8191 exactly once")

    totals = {
        field: sum(int(row[field]) for row in summaries)
        for field in ("sent", "delivered", "echoed")
    }
    overall = overall_rows[0]
    if any(totals[field] != int(overall[field]) for field in totals):
        raise ValueError("Per-station and run totals disagree")
    if totals != {"sent": 8191, "delivered": 8161, "echoed": 8161}:
        raise ValueError(f"Unexpected reference totals: {totals}")

    incomplete = [
        row for row in summaries
        if row["sent"] != row["delivered"] or row["delivered"] != row["echoed"]
    ]
    if len(incomplete) != 30:
        raise ValueError(f"Expected 30 incomplete STAs, found {len(incomplete)}")

    for folder in ("original", "normalized", "metadata", "evidence", "provenance"):
        (output / folder).mkdir(parents=True, exist_ok=True)
    for name in ("run-beacons.csv", "run-phy.csv", "run-run.csv", "run-summary.csv", "run-tx.csv"):
        shutil.copy2(run / name, output / "original" / name)
    for name in ("binary-sha256.txt", "command.sh", "execution.json", "raw.txt",
                 "resources.jsonl", "run-ap-pending.log", "source-hashes.json"):
        shutil.copy2(run / name, output / "metadata" / name)

    write_csv(output / "normalized" / "stations.csv", summaries,
              ["aid", "associated", "sent", "delivered", "echoed"])
    write_csv(output / "normalized" / "incomplete-stations.csv", incomplete,
              ["aid", "associated", "sent", "delivered", "echoed"])

    repo = run.parents[1]
    evidence = (
        repo / "validation/halow-8191/association-state-2026-10-16/evidence.json",
        repo / "validation/halow-8191/uplink-diagnostics-2026-10-13/result-8191-path.json",
    )
    for source in evidence:
        shutil.copy2(source, output / "evidence" / source.name)
    shutil.copy2(Path(__file__), output / "provenance" / Path(__file__).name)

    validation = {
        "source_run": run.name,
        "status": "passed",
        "purpose": "audit_of_incomplete_reference_before_association_state_fix",
        "stations": 8191,
        "associated_reported": int(overall["associated"]),
        "sent": totals["sent"],
        "delivered": totals["delivered"],
        "echoed": totals["echoed"],
        "incomplete_stations": len(incomplete),
        "incomplete_aids": [int(row["aid"]) for row in incomplete],
        "full_run_log_included": False,
        "network_snapshot_included": False,
    }
    (output / "validation.json").write_text(json.dumps(validation, indent=2) + "\n")
    (output / "README.md").write_text("""# Referência anterior de 8.191 STAs — 13/10/2026

Este pacote permite auditar a execução anterior à correção do estado de
associação no AP. A simulação terminou com código zero e registrou 8.191 STAs
associadas, mas apenas 8.161 das 8.191 requisições chegaram ao AP e retornaram
à aplicação. `normalized/incomplete-stations.csv` identifica as 30 STAs.

Os CSVs originais, o comando, os parâmetros, os hashes das fontes e as
evidências diagnósticas estão preservados. `evidence/evidence.json` mostra que
essas STAs haviam concluído a associação, enviaram novos pedidos de associação
e depois alcançaram o AP com `associated=0`. Essa evidência fundamentou a
correção aplicada à referência final de 16/10/2026.

`run.log` e `run.nss` foram omitidos porque somam aproximadamente 257 MB. Eles
não são necessários para conferir os totais, as STAs incompletas ou a cadeia
diagnóstica selecionada. Este pacote documenta uma referência histórica com
falha e não constitui o resultado final de sucesso.

Confira todos os arquivos com `manifest-sha256.json`. A referência funcional
final está no artefato `dataset-seeds-1-2-3-2026-10-20.zip` da mesma release.
""")

    manifest = {}
    for path in sorted(output.rglob("*")):
        if path.is_file():
            manifest[str(path.relative_to(output))] = {
                "sha256": digest(path),
                "bytes": path.stat().st_size,
            }
    (output / "manifest-sha256.json").write_text(json.dumps(manifest, indent=2) + "\n")

    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zipped:
        for path in sorted(output.rglob("*")):
            if path.is_file():
                zipped.write(path, str(Path(output.name) / path.relative_to(output)))
    print(json.dumps({
        "output": str(output),
        "zip": str(archive),
        "bytes": archive.stat().st_size,
        "sha256": digest(archive),
        **validation,
    }, indent=2))


if __name__ == "__main__":
    main()
