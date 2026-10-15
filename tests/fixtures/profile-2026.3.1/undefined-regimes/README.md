# Undefined regime cells

This 587-byte subset comes from the actual 2026.3.1.0/build 38722833 export
retained by I-040. It preserves six numeric columns and two data rows, including
duplicate headers, exact numeric text and two literal `nan` cells. See
`provenance.json` for source/fixture hashes, line/column selection and the pinned
investigation. The source capture remains explicitly failed.

The qualified parser returns no number for those cells. Finite-only parsing,
other nonfinite spellings and nonfinite timing/frame values remain rejected.
This fixture proves parser behavior, not hardware compatibility, counter units
or a cause for missing measurements.
