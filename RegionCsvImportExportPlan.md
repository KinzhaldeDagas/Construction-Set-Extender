# Feature Plan: Region Data CSV Export / Import

## Goals
- Add **round-trip export/import** of Region data using CSV files so authors can batch-edit data in external tools.
- Keep exported/imported references safe by default:
  - Export only entries that resolve to forms available from the **active plugin** or its **loaded parent masters**.
  - On import, reject or report rows referencing unavailable forms.
- Integrate into existing Region Editor workflows with minimal disruption.

## Scope
### In scope
- Region data categories:
  - General metadata (region identity / editor-facing metadata)
  - Objects (rocks/flora/static placements / chance/radius data)
  - Grass
  - Landscape textures
  - Weather / sounds (optional Phase 2, see rollout)
- CSV schema definition and versioning.
- Import validation + dry-run preview.
- Structured error reporting.

### Out of scope (initial)
- Non-CSV formats (JSON/XLSX).
- Auto-fetching missing assets from unloaded plugins.
- Reconciliation UI for unresolved references beyond skip/fail policies.

## User Stories
1. As a level designer, I can export one region or all selected regions to CSV and edit in spreadsheet tooling.
2. As a plugin author, I can import the edited CSV back and apply changes safely.
3. As a mod maintainer, I can constrain export to forms available in the active plugin/master chain to avoid invalid cross-plugin references.
4. As QA, I can run import in dry-run mode and see deterministic warnings/errors before applying writes.

## UX / UI Plan
- Add buttons to Region Editor:
  - `Export CSV...`
  - `Import CSV...`
  - `Import CSV (Dry Run)`
- Add export options dialog:
  - Region selection: current region / filtered selection / all.
  - Data sections checkboxes: Objects, Grass, Landscape Textures, Weather, Sounds.
  - Toggle: `Only include assets available in active plugin + masters` (default ON).
- Import summary dialog:
  - Rows processed / created / updated / skipped / failed.
  - Missing form references list.
  - Save report to `*.log`.

## CSV Data Model
Use one CSV per section to keep schemas stable and readable.

### 1) `regions.csv`
- `RegionEditorID`
- `RegionFormID`
- `DisplayName`
- `Priority`
- `Flags`

### 2) `region_objects.csv`
- `RegionEditorID`
- `EntryType` (`Tree|Rock|Flora|Static|Other`)
- `FormID`
- `EditorID`
- `Chance`
- `Density`
- `MinSlope`
- `MaxSlope`
- `MinHeight`
- `MaxHeight`

### 3) `region_grass.csv`
- `RegionEditorID`
- `GrassFormID`
- `GrassEditorID`
- `Density`

### 4) `region_landscape_textures.csv`
- `RegionEditorID`
- `TextureFormID`
- `TextureEditorID`
- `MinSlope`
- `MaxSlope`

## Filtering Rule (Active Plugin + Parent Masters)
Define a reusable resolver utility:
1. Build set of loaded files in scope: active plugin + recursive masters.
2. For each referenced form in export candidates:
   - Resolve owning file.
   - Include row only if owner file is in scope set.
3. Emit skip counts by section and owner plugin for transparency.

Import uses the same resolver:
- If row references form outside scope or unresolved form ID/editor ID, mark row invalid.
- Depending on import mode:
  - Dry-run: report only.
  - Apply: skip invalid row and continue (default) or abort on first error (advanced option).

## Engine Integration Notes
Potential integration points in existing codebase:
- Region editor hook/subclass infrastructure in `Hooks/Hooks-Dialog.*`.
- Region editor dialog identification via `TESDialog::kDialogTemplate_RegionEditor` and related templates.
- Editor API form typing and region classes in `EditorAPI/TESForm.h` and RTTI declarations.

Recommended structure:
- `RegionCsvIO.h/.cpp`
  - Serialization/deserialization helpers.
  - CSV parsing/writing (escape-safe).
  - Validation + report object.
- `RegionAssetScopeResolver.h/.cpp`
  - Active plugin/master scope builder.
  - Form availability checks.
- UI glue in region dialog hooks to invoke export/import workflows.

## Import Algorithm (Apply Mode)
1. Parse files + validate headers/schema version.
2. Resolve regions by `RegionFormID`, fallback `RegionEditorID`.
3. For each section row:
   - Validate scalar fields/ranges.
   - Resolve referenced form.
   - Verify form owner plugin in allowed scope.
   - Upsert into corresponding region data list.
4. Record per-row outcome into report.
5. Refresh Region Editor controls and mark plugin modified.

## Error Handling
- Categorize errors:
  - `SchemaError` (missing columns/version mismatch)
  - `ParseError` (bad number/enum)
  - `ResolveError` (unknown form/editor ID)
  - `ScopeError` (form not in active plugin/master chain)
  - `ApplyError` (engine write/update failed)
- Always include file + row + column in report.

## Rollout Plan
### Phase 1 (MVP)
- Export/import for Objects, Grass, Landscape Textures.
- Scope filter enforced.
- Dry-run + apply report.

### Phase 2
- Add Weather + Sounds.
- Add configurable conflict policies (replace/merge/append).
- Add CLI/script command entrypoint for automation.

## Test Plan
- Unit-style checks for CSV parsing/escaping and schema version checks.
- Resolver tests:
  - Form in active plugin -> allowed.
  - Form in master -> allowed.
  - Form in unrelated loaded plugin -> blocked.
- Integration tests (manual/editor):
  - Export region with mixed references and verify filtered rows.
  - Import valid CSV round-trip and verify no data drift.
  - Import with bad rows and verify deterministic skip/error reporting.

## Risks / Mitigations
- **Risk:** FormID collisions across plugin context.
  - **Mitigation:** Prefer full resolver path with file ownership checks; support EditorID fallback only with strict uniqueness check.
- **Risk:** CSV edits breaking enums/ranges.
  - **Mitigation:** Strict validation + human-readable report + dry-run default.
- **Risk:** Partial imports leaving inconsistent region data.
  - **Mitigation:** Transaction-like staging in memory before apply where practical; otherwise section-level rollback on fatal errors.

## Acceptance Criteria
- User can export selected region data to CSV files.
- User can import the CSV files and reapply edits.
- Exported Objects/Grass/Landscape rows include only forms available in active plugin + parent masters when filter is enabled (default).
- Import report clearly lists skipped/failed rows and reasons.
- Dry-run mode performs no writes while producing same validation report shape as apply mode.
