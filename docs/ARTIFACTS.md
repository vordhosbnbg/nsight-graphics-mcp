# Managed artifact storage

The C++ `ngm::ArtifactStore` in `include/ngm/Artifacts.hpp` owns capture evidence
independently of MCP and the capture backend. It uses JSON manifests and ordinary
directories; no SQLite dependency or private Nsight format is involved.
The storage core does not establish that a capture, export, diagnosis, or
comparison has succeeded. The producing backend supplies that evidence and its
provenance.

## Layout and publication

Every bundle receives a random, stable bundle- ID containing 128 random bits.
The root contains a versioned ownership marker and an exclusive flock lock:

~~~text
.ngm-store.json
.lock
staging/<id>/manifest.json
staging/<id>/pin.json
staging/<id>/raw/
staging/<id>/derived/
bundles/<id>/...          # Published complete or failed bundles
expired/<id>.json         # Small records explaining expired references
trash/<id>/...            # Claimed deletion awaiting filesystem cleanup
~~~

`begin()` creates staging on the destination filesystem. Callers write original
captures, exports, logs, shader sources, and reports under raw/; derived indexes
belong under derived/. `update_provenance()` accepts final backend/tool,
application/build, shader-hash, settings, GPU/driver, desktop, and unit metadata
before publication. These details are backend supplied, not guessed by the store.
The manifest records the project version from `ngm::project_version()` separately
from its own schema version, currently 1.

The producing worker must stop all owned output producers and close their output
descriptors before calling `publish_success()` or `publish_failure()`.
Publication validates paths, file types, inventory bounds, and manifest fields.
Required success outputs must exist as nonempty regular files. Failed bundles may
lack required outputs and carry an explicit reason. Files and metadata are synced
before staging is renamed into bundles/; the containing directories are synced
after the rename. Complete bundles cannot be observed through the API before the
rename. Published raw/derived files and provenance are immutable by contract.
Pin state is the separately mutable metadata.

The writer remains protective across publication until it is destroyed or reset.
A job coordinator can therefore accept a worker completion before releasing the
writer, without a prune race between publication and completion processing.
`directory()` follows the writer from its staging location to the published
location; it does not grant permission to modify published files.

## Pins, leases, and retention

The default limits are **2 GiB** of logical file bytes and **30 days** after
completion. `ArtifactOptions::max_bytes` and max_age override them; zero disables
the respective limit. Wall-clock timestamps are persisted as Unix milliseconds.
Age is measured from completion, independently of directory modification times
and last access. A backward clock movement does not prematurely age a bundle.
Equal completion timestamps use artifact ID as a deterministic pruning tie-break.

These defaults are provisional. An inspection on 2026-09-18 measured 68 existing
artifacts/fixture-validation/*/report.json run directories:

| Measurement | Logical bytes |
| --- | ---: |
| Smallest run directory | 108,815 |
| Largest run directory | 6,292,784 |
| Median run directory | 6,217,244.5 |
| Total | 319,520,170 |

These are **application-readback fixture artifacts**, including retained
executables/configuration where present. The six later basic capture bundles in
[NSIGHT_VALIDATION.md](NSIGHT_VALIDATION.md) measure **284,150–286,838 bytes**
each, including captures, exports, logs, reports, and available application
provenance. They are small basic workloads, not representative trace or large
application measurements. The selected 2 GiB/30-day defaults remain configurable;
revisit them after the advanced and profiling workloads are measured.

`pin(id, true)` persists protection; `pin(id, false)` releases it. Pinning does not
remove bytes from usage. Investigation evidence and verification baselines must
be explicitly pinned: an artifact reference alone provides no protection.
`import_directory()` copies an existing evidence directory into raw/imported/
and defaults to pinned. It never changes the source or follows source links.
Its required paths are relative to the source directory.

`lease(id)` returns a move-only RAII handle for reading or comparison. The handle
keeps the store ownership lock alive as well as protecting its bundle. `read()`
acquires its own lease. Metadata inspection is coordinated with deletion claims.
Pin updates, lease acquisition, and deletion claims share one mutex. Pruning
rechecks current protection when claiming each candidate, after candidate
selection; it never relies on an earlier snapshot of pins or leases.

Pruning removes the oldest eligible published bundles until the byte budget is
met, and independently removes every eligible bundle whose maximum age has been
reached. Both complete and explicit failed bundles are eligible. Active writers,
quarantined attempts, pinned evidence, and leased bundles are protected. Every
deletion first persists an expiration record and atomically moves the entire
bundle to trash/ while holding the protection mutex. Recursive deletion happens
after releasing that mutex. A failed deletion remains charged and is retried on
the next prune or restart.

The core prunes at `begin()`, before successful publication, and when `prune()` is
called explicitly. It has no background cleanup thread. The embedding server
chooses any additional scheduling. Successful publication predicts the final
manifest and includes its growth when selecting bundles to prune, then checks
the actual resulting quota again while holding the protection mutex.

Usage includes complete/failed bundles, staging, protected data, expiration
records, root metadata, and pending trash. Pinned and in-use byte fields are
overlapping subsets, not additional totals. Trash is conservatively charged while
deletion is running and after a failed deletion, so callers never assume space
was freed merely because deletion was claimed. The measure is logical file size,
not allocated filesystem blocks, directory-entry overhead, or filesystem free
space; sparse files, compression, and filesystem metadata can differ.

Protected data can exceed the configured budget. Writers and failed attempts
are retained in that situation. New writes through an external capture process
cannot be capped by this API; successful publication enforces the byte budget,
while explicit failed publication preserves available evidence even above it.
quota_exceeded explains admission refusal and `prune()` reports remaining
overage. Freeing protected evidence requires an explicit unpin or lease release;
the store never discards it to satisfy a limit.

## Restart and unconfirmed cleanup

An exclusive store lock rejects a second process or store instance with busy.
The lock remains owned until all store, writer, and lease handles close.
Applications must not unlink the lock file or modify the managed root externally.

On startup the core validates published manifests, inventories, and pins.
A valid final manifest still in staging is a prepared publication: its files are
revalidated and its atomic rename is completed. A staging attempt without a valid
final manifest becomes a **failed, pinned, quarantined** record in staging.
Malformed initial manifests are retained under raw/ when possible. Unsafe
metadata/filesystem structures cause an explicit error and are preserved.

`quarantine(writer, reason)` supplies the same protection when a worker cannot
confirm owned-process cleanup. A writer closed without final publication is
quarantined as well. Quarantined records appear in bounded metadata with
status "failed" and quarantined true; they are not published evidence and
cannot be read, leased, unpinned, or automatically pruned. Their bytes remain in
staging usage. Restart never assumes a child process died with the server.

After the owner has verified process cleanup, a live writer or a writer obtained
through `resume_quarantined(id)` can call `publish_failure()` to validate and
publish its retained evidence. This preserves the pin, which can subsequently be
released explicitly. A quarantined attempt cannot be promoted to success.
The core cannot independently verify ownership or cleanup of a backend process;
that is the job coordinator's responsibility.

An expiration receipt without a committed move is discarded if the complete
bundle remains visible. A receipt with a bundle in trash resumes deletion.
An ambiguous duplicate identity or missing receipt stops recovery rather than
guessing which data can be removed. Expiration records are retained indefinitely
and included in usage, so old references continue to explain their removal.

## Bounded APIs and error handling

`inspect()` returns summary/provenance and a file count; `list()` returns at most
100 summaries and an ID cursor. `files()` returns at most 100 entries and an
offset cursor. `read()` defaults to 1 MiB and permits an explicit bound up to
16 MiB for inventory analysis. It reads only inventoried raw/derived
regular files. Larger artifacts remain on disk for consumers holding leases.
An expired `inspect()` returns status "expired" and its reason; attempting
to read, pin, or lease it returns the typed expired error.

Provenance is limited to 64 KiB, manifests to 4 MiB, required paths to 128,
inventoried files to 4,096, total inventoried directory entries to 16,384,
directory nesting to 64 levels below a bundle root, and individual relative paths
to 512 bytes. The raw/ and derived/ directories count as the first nesting level;
raw/imported/ counts as two levels and its imported/ wrapper counts as an
inventoried directory entry. Failure reasons are limited to 4,096
bytes. These bounds are independent of retention limits. Import checks the same
file, entry, and depth bounds before creating an offending output, so a limit
failure retains a valid normal failed bundle with the requested pin state.
Accounting traverses staging iteratively even if an external writer has exceeded
publication depth bounds; metadata inspection and other admissions remain usable
while that writer is corrected or quarantined.

Errors distinguish invalid_argument, not_found, expired, busy,
quota_exceeded, disk_full, corrupt, and filesystem. POSIX `ENOSPC` and
`EDQUOT` map to actionable disk_full errors. A failed pin synchronization
conservatively protects the bundle in memory. A failed import retains its
available evidence and includes the attempt ID in the error.

Directory traversal uses descriptor-relative operations and refuses symlinks at
every opened directory component. Publication rejects symlinks, devices, FIFOs,
and multiply linked regular files. Cleanup is limited to claimed managed
directories and never follows a link. An existing nonempty directory without the
store ownership marker is not adopted. The deployment contract is a local Linux
filesystem honoring flock, atomic same-filesystem rename, and fsync.
Remote/network filesystems and hostile concurrent mutation by another process
running as the same user are not supported. Callers must not mutate published
bundles or the store's internal metadata.

## Validation scope

`tests/ArtifactsCheck.cpp` uses temporary directories and a fake clock. It checks
publication visibility, required output validation, stable IDs/timestamps,
persistent pin/unpin and expiration records, pagination, lease and writer
lifetimes, age/budget ordering, protected quota exhaustion, quarantine/recovery,
imports, path/link/FIFO rejection, and ownership conflicts. Regression cases
cover final-manifest growth that requires pruning below the current-byte limit,
protected evidence during that pruning, exact directory-depth boundaries,
excessive directory nesting in external staging, and a 4,097-file import that
retains a valid unpinned failed bundle across restart and normal expiration.

A synchronized threaded check acquires a pin and lease after pruning has chosen
candidates and before it claims deletion. Child processes exit without running
destructors to exercise unfinished and prepared-publication crash windows.
Test-only linker wrappers inject `ENOSPC`, a directory-sync failure after a pin
rename, a publication rename failure, and a recursive unlink failure at real POSIX
syscall boundaries. A second synchronized check pins a bundle while another
bundle is being deleted, and verifies retained trash accounting and retry.
No filesystem is filled and no system settings are changed.

The isolated GCC 16.2.1 C++20 build and test run passed with
`-Wall -Wextra -Wpedantic -Werror`; the same checks passed with AddressSanitizer
and UndefinedBehaviorSanitizer. Repository CMake integration is supplied by
`cmake/Artifacts.cmake`. CPU storage tests establish these core contracts;
MCP integration, real capture retention, pinned investigation references, and
correct/faulty comparison retention are covered separately by the completed
R-012 acceptance record in [BUILD_VALIDATION.md](BUILD_VALIDATION.md). The
20-check integrated CPU aggregate passes; the two real basic capture matrices
verify pin persistence and retrieval after server restart. This storage
acceptance does not imply source-repair or GPU replay success.
