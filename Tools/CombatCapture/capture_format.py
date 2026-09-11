"""Katana capture-format selection and translation into portable analysis APIs."""
from pathlib import Path

import visual_analysis  # Repository bootstrap lives outside the portable package.
from animation_analysis.artifacts import digest, file_manifest, identity, implementation_manifest
from animation_analysis.contracts import ClockStamp
from animation_analysis.temporal import TimeInterval, bracket_observations, event_window


def input_names(root, include_scenario=False):
    """Legacy bundle selection; derived reports never identify their own inputs."""
    root = Path(root)
    metadata = {"session.json"}
    if include_scenario:
        metadata.update(("scenario.json", "run-context.json", "asset-identity.json"))
    return [p.relative_to(root) for p in root.rglob("*") if p.is_file()
            and (p.suffix in (".jsonl", ".csv", ".png") or p.name in metadata)]


def bundle_identity(root):
    return identity(file_manifest(root, input_names(root, include_scenario=True)))


def implementation_identity(entrypoint):
    """Identify offline analysis code separately from captured execution state.

    Legacy references hashed only one script and are intentionally incompatible
    with this implementation. Hash the shared package as well as this format
    adapter, analyzer and bootstrap so dependency changes cannot reuse a reference.
    This does not identify external decoders or the editor binary.
    """
    directory = Path(__file__).resolve().parent
    files = (Path(entrypoint).resolve(), Path(__file__).resolve(),
             directory / "analyze_capture.py", directory / "visual_analysis/__init__.py")
    project = {path.relative_to(directory).as_posix(): digest(path) for path in files}
    return identity({"project": project, "package": implementation_manifest()})


def event_interval(events, first, last, before=0.0, after=0.0):
    interval = event_window(((row["marker"], ClockStamp("simulation", row["simulation_time_s"]))
                             for row in events), first, last, before, after)
    return [interval.start.seconds, interval.end.seconds]


def window_rows(rows, interval):
    return bracket_observations(rows, TimeInterval(ClockStamp("simulation", interval[0]),
                                                   ClockStamp("simulation", interval[1])),
                                lambda row: ClockStamp("simulation", row["simulation_time_s"]))
