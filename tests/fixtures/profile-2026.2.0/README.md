# GPU Trace parser fixtures

Selected real exports from Nsight 2026.2.0.0/build 37991608, Vulkan on RTX 3080 Ti
with driver 615.71.09, Throughput Metrics, unaltered clocks and single-pass
collection. Files named .xls by Nsight contain UTF-8 TSV. Host/process context is
removed; numeric text, positions and duplicate event labels are preserved.
provenance.json describes extraction. These are CPU parser inputs, not evidence
of application-only counters or a requirement to retain the original traces.
