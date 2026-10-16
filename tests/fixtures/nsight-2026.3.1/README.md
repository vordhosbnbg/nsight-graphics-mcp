# Nsight metadata fixtures

Sanitized real Vulkan exports from Nsight 2026.3.1.0/build 38722833, used by CPU
parser checks. Process paths/environment are removed; inventory values, ordering,
warning text and optional fields are preserved. Indirect examples retain the
producer's indirect_index without inventing command-expansion relationships.
Compute metadata preserves its empty primary_api and Vulkan graphics_apis.
These inputs exercise parsing; they do not prove GPU execution or state coverage.
See [Compatibility](../../../docs/COMPATIBILITY.md).
