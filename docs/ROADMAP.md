# Roadmap

Only unfinished accepted work belongs here. Completed work is described by the
current guides; Git preserves its history. Keep items under their matching
status section with the fields below. An empty list does not authorize new priorities.

## Blocked

### R-026

```text
Status: Blocked
Area: profile/analysis/mcp/test
Title: Qualify hardware counter meaning and application attribution
Goal: Make supported counters trustworthy evidence of application work.
Scope: Establish documented version/architecture-specific units, normalization, aggregation and application boundaries; explain or bound differing totals for equivalent workloads.
Acceptance: Independent new-input workloads verify each advertised interpretation and collection scope. Draw/fragment claims require proven occurrence/stage correspondence. Preserve raw zero versus unavailable cells and test persistence. An unresolved attribution report is not completion.
Notes: Nsight Graphics only; a separate Perf SDK backend is outside scope. Available exports and native multi-pass collection have not established the missing definitions/application boundary. Resume when a usable documented Graphics interface or authoritative definition supplies that prerequisite. See COMPATIBILITY.md.
```
