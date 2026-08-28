# Revoice CSV Exporter: Missing Voice Line Troubleshooting

## Symptom
Some valid dialogue lines are missing from CSV exports, including entries like **"Latest Rumor"**, even when all numbered variants (`_1`, `_2`, `_3`, etc.) exist in the source data.

## Most likely cause
The exporter is probably collapsing records using a **base topic key** (for example, stripping trailing `_N` suffixes) and then retaining only a single record per key.

That can remove legitimate lines when:
- multiple NPCs share the same topic text,
- the same line family exists in different quests/contexts,
- all numbered variants are valid and intended (not just orphaned `_2`/`_3` entries).

## Recommended export key
To avoid accidental deduplication, use a stable per-response identity, not only display text or normalized topic text.

### Chosen direction (Option B)
Use a **string signature with explicit schema versioning** and include the **spoken race** in the signature so Revoice can reliably detect/export rows without inferring from paths or display text.

Recommended fields:
- `revoice_signature` (string)
- `revoice_signature_version` (integer, start at `1`)
- `spoken_race` (stable sanitized key, EditorID preferred; fallback FormID)

Suggested signature template (`v1`):
`rvx:v1|plugin=<plugin>|quest=<quest>|topic=<topic>|info=<info>|response=<index>|race=<spoken_race>`

Notes:
- Keep `spoken_race` explicit in a CSV column even if it is also encoded inside `revoice_signature`.
- Treat the full `revoice_signature` as the dedupe key; never dedupe by visible line text alone.
- Bump `revoice_signature_version` when composition rules change.

A safe identity should include at least:
- plugin/source file,
- quest identifier,
- topic identifier,
- INFO/form identifier (or equivalent unique response id),
- response index/number,
- spoken race.

## Filtering guidance
If you need to hide obvious junk rows, avoid dropping entire `_N` families by text alone.

Prefer this rule:
1. Group by the full unique response identity.
2. Keep all numbered variants if `_1` is present for that identity.
3. Only flag `_2`/`_3` rows as suspicious when `_1` is missing for the **same identity**.

## Quick validation checklist
Run these checks against known-problem topics (e.g., "Latest Rumor"):
- [ ] Compare count in editor vs. exported CSV.
- [ ] Verify shared-NPC lines are all present.
- [ ] Confirm `_1/_2/_3` rows remain grouped under the same base line but exported as separate rows.
- [ ] Confirm no cross-quest or cross-topic merge occurs for same visible text.


## Patch plan
1. **Stop bucketing by `voiceid` in output paths.**
   - Current behavior to avoid: `.../<voiceid>/...` directory partitioning.
   - Required behavior: `.../<race>/...` directory partitioning.
   - Rationale: shared lines across NPCs/voice IDs should still collate under race-level output, which is how downstream processing expects paths.
2. Keep the CSV row identity strict (plugin + quest + topic + INFO/form + response index + spoken race), represented by `revoice_signature`, so path changes do not reintroduce dedup loss.
3. Add a regression check with known lines (e.g., "Latest Rumor") across NPCs sharing content to ensure no row drops when race-bucket output is used.
4. Add CSV columns `revoice_signature`, `revoice_signature_version`, and `spoken_race`, and validate that every exported row has non-empty values.

## Proposed path rule
- Replace path builder logic from:
  - `output_root/<voiceid>/<topic_or_line>.csv`
- To:
  - `output_root/<race>/<topic_or_line>.csv`

Where `race` should be a stable, sanitized race key (EditorID preferred; fallback to FormID string).

## Acceptance criteria
- [ ] Exported files are written under race directories, not voice-id directories.
- [ ] Shared-topic lines are preserved even when NPC voice IDs differ.
- [ ] "Latest Rumor" and similar `_1/_2/_3` families export fully for affected NPCs.
