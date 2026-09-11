"""Summarize repeatability/overhead and optionally create explicitly selected mechanical references."""
import argparse
from collections import defaultdict
from pathlib import Path

import visual_analysis  # Repository bootstrap.
from animation_analysis.errors import EvidenceError as CaptureError
from animation_analysis.integrity import read_json
from animation_analysis.artifacts import atomic_json
from animation_analysis.metrics import distribution as numeric_distribution
from capture_format import bundle_identity


def summarize(batches):
    grouped = defaultdict(list)
    for path in batches:
        batch = read_json(path / "batch.json" if path.is_dir() else path)
        for run in batch["results"]:
            grouped[(run["map_key"], run["variant"], run["mode"], run["render_backend"])].append(run)
    result = []
    for (map_key, variant, mode, backend), runs in sorted(grouped.items()):
        passing = [r for r in runs if r["status"] == "pass"]
        def distribution(values):
            measured = numeric_distribution(values)
            return {key: measured[key] for key in ("min", "median", "max")} if measured["count"] else None
        result.append(dict(map_key=map_key, variant=variant, mode=mode, render_backend=backend, runs=len(runs), passing=len(passing),
                           wall_duration_s=distribution([r["wall_duration_s"] for r in passing]),
                           simulation_duration_s=distribution([r["simulation_duration_s"] for r in passing]),
                           paired_duration_s=distribution([r["timing"]["paired_observation_duration_s"]["value"] for r in passing if r["timing"].get("paired_observation_duration_s")]),
                           max_point_step_cm=distribution([r["measurements"]["transition"]["max_actor_relative_step_cm"] for r in passing if "transition" in r["measurements"]]),
                           captures=[r.get("capture") for r in runs]))
    return dict(groups=result, interpretation="Small diagnostic replay sample. Compare gameplay durations/outcomes as well as wall time. Isolate recorder overhead only between runs using the same render backend, source and assets.")


def create_reference(captures, margin_cm, basis):
    if len(captures) < 3 or margin_cm <= 0 or not basis.strip():
        raise CaptureError("Reference requires at least three eligible unchanged runs, a positive declared margin and acceptance basis")
    evaluations = [read_json(path / "evaluation.json") for path in captures]
    if any(bundle_identity(path) != evaluation["bundle_identity"] for path, evaluation in zip(captures, evaluations)):
        raise CaptureError("A reference recording changed after evaluation")
    if len({e["run_id"] for e in evaluations}) != len(evaluations):
        raise CaptureError("Duplicate recordings cannot establish repeatability")
    first = evaluations[0]
    for evaluation in evaluations:
        if evaluation["status"] != "pass" or evaluation["control_offset_cm"] != 0:
            raise CaptureError("Reference examples must pass and have no injected defect")
        if evaluation["compatibility"] != first["compatibility"] or evaluation["evaluator_identity"] != first["evaluator_identity"] or evaluation["provenance"] != first["provenance"]:
            raise CaptureError("Reference examples changed scenario, evaluator, source or assets")
        for required in ("pose.window_evidence", "visual.window_evidence", "run.identity"):
            if not any(c["name"] == required and c["status"] == "pass" for c in evaluation["cases"]):
                raise CaptureError("Reference examples require eligible rendered and pose evidence")
    values = [e["measurements"]["transition"]["max_actor_relative_step_cm"] for e in evaluations]
    return dict(reference_schema_version=1, compatibility=first["compatibility"], evaluator_identity=first["evaluator_identity"],
                provenance=first["provenance"], observed_maxima_cm=values, margin_cm=margin_cm,
                max_actor_relative_step_cm=max(values)+margin_cm, acceptance_basis=basis,
                meaning="A sampled displacement envelope for this scenario/point/resolution; neither a universal defect threshold nor artistic approval",
                examples=[dict(run_id=e["run_id"], bundle_identity=e["bundle_identity"], capture=str(p)) for e, p in zip(evaluations, captures)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("batches", nargs="*", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference-captures", nargs="+", type=Path)
    parser.add_argument("--margin-cm", type=float, default=10)
    parser.add_argument("--basis", default="")
    args = parser.parse_args()
    if args.reference_captures:
        if args.output.exists():
            parser.error("Reference already exists; choose a new version/path and document why it replaces the old one")
        value = create_reference(args.reference_captures, args.margin_cm, args.basis)
    else:
        value = summarize(args.batches)
    atomic_json(args.output, value)
    print(args.output)


if __name__ == "__main__":
    main()
