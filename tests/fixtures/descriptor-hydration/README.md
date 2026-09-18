# Trusted descriptor-hydration cases

`cases.json` identifies four previously retained, pinned C++ captures and the
exact helper/database/source bytes reviewed for the opt-in
`ngm_descriptor_hydration_integration` experiment. It contains hashes and small
source callsites only; generated helper code, captures, and proprietary binaries
are not checked in. These are real evidence identities, not sanitized fake
producer inputs and not a generic resource-extraction allowlist.

The runner snapshots the verified helper closure and compiles it unchanged for
each case. The worker checks all input identities before initialization. Changing
this inventory requires review and a harness rebuild, then fresh qualification;
new hashes alone do not establish safety for novel payloads. Expected descriptor
fields live in the test harness and are not exposed as MCP evidence.

See [the contract and reproduction instructions](../../../docs/DESCRIPTOR_HYDRATION.md).
