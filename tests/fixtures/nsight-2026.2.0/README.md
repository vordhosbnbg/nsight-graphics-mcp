# Nsight metadata fixtures

Sanitized real Vulkan metadata/function/object exports from Nsight
2026.2.0.0/build 37991608, used by CPU parser checks. Process paths/environment
are removed; inventory ordering, values and optional-field absence are preserved.
The indirect examples omit indirect_index, unlike the companion 2026.3 fixtures.
Opaque access flags must not become inferred bindings or GPU state.
See [Compatibility](../../../docs/COMPATIBILITY.md) for supported interfaces.
