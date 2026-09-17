# Nsight Integration Investigations

Record each failed Nsight integration or evidence-extraction attempt for future
investigation, as requested in planning round 5. Include inconclusive probes;
distinguish an observed failure from a proven limitation. The current integration
scope uses documented Nsight interfaces only.

Related planning: [ROADMAP.md](ROADMAP.md), especially R-006 and its dependent
diagnostic workflows. Guidance and the dated local baseline: [AGENTS.md](../AGENTS.md).

## Recording rules

- Give each failed attempt a stable ID and its own entry. Link related attempts
  and successful follow-ups; preserve the original result when resolving a gap.
- Record enough context to reproduce the attempt: documented entry point and
  source, exact arguments or a minimal SDK example, workload/build identity,
  configuration, and tool/SDK/GPU/driver versions as relevant.
- Describe expected and observed results separately, including exit status and
  the relevant error or missing data. Retain raw evidence as local artifacts;
  commit only small, sanitized excerpts or fixtures, not large captures/logs.
- Automatic artifact retention was selected in round 8. Explicitly pin the evidence
  bundles relied on by each entry and record their IDs/retention state; a link does
  not itself protect data from pruning. If evidence is intentionally removed later,
  update the entry to mark it unavailable while preserving the reproduction steps
  and sanitized findings. R-012 tracks the storage and pinning implementation.
- State the narrow conclusion supported by the evidence: a configuration error,
  prerequisite, version-specific failure, missing documented path, or inconclusive
  result. Do not label a capability universally unsupported from one failed probe.
- Name the affected roadmap item/capability, current limitation, and a useful
  revisit condition, such as a new documented interface, tool version, or corrected
  prerequisite. Investigation records do not authorize excluded integration paths
  or count as implementation of a missing capability.

## Attempts

No capture, replay, SDK-control, or profiling experiments have been run yet.
The existing baseline comes from documentation, CLI help/version output, and SDK
header inspection; those observations are recorded in AGENTS.md. No failed runtime
attempts are asserted by this initial log.

## Entry template

Use the next unused I-### ID. An unsuccessful retry gets a new entry linked to the
earlier attempt so changes in method or environment remain visible.

```text
ID: I-001
Date:
State: Open | Resolved | Superseded
Related roadmap items and attempts:
Capability sought:
Documented interface and source:
Environment: Nsight/SDK versions, GPU/driver, and relevant platform details.
Workload: Scenario, source/build identity, settings, and input artifacts.
Reproduction: Exact command arguments or minimal SDK example and prerequisites.
Expected result:
Observed result: Exit status, error, or missing data.
Evidence: Artifact locations and a small sanitized excerpt where useful.
Retention: Explicitly pinned evidence-bundle IDs, or unavailable evidence and why.
Conclusion: Supported finding and remaining uncertainty.
Current limitation:
Revisit condition:
Resolution: Leave open until supported by a linked successful follow-up.
```
