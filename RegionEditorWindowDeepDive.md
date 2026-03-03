# Region Editor Window Deep Dive (Construction Set Extender)

This document maps what can be **proven from this codebase** about the Region Editor window, each tab, tab-specific data surfaces, and the CSV tooling added by CSE.

## 1) Region Editor window and tab templates

The Region Editor main dialog template is `kDialogTemplate_RegionEditor = 250`. The tab content panes are individual dialog templates:

- `kDialogTemplate_RegionEditorGeneral = 253`
- `kDialogTemplate_RegionEditorMapData = 256`
- `kDialogTemplate_RegionEditorWeatherData = 251`
- `kDialogTemplate_RegionEditorObjectsData = 252`
- `kDialogTemplate_RegionEditorObjectsExtraData = 309`
- `kDialogTemplate_RegionEditorLandscapeData = 3214`
- `kDialogTemplate_RegionEditorGrassData = 3220`
- `kDialogTemplate_RegionEditorSoundData = 3233`

These identifiers define the tab-level partitioning of Region data in the editor runtime and are the stable handles used by CSE hooks/subclass registration.

## 2) Tabs and what this repo confirms for each one

## General
- Template ID: `253`.
- Confirmed as a Region Editor subwindow template.
- No CSE custom subclass behavior is attached specifically to this template in this repository.

## Map
- Template ID: `256`.
- Confirmed as a Region Editor subwindow template.
- No CSE custom CSV import/export overlay is attached to this template.

## Weather
- Template ID: `251`.
- Confirmed as a Region Editor subwindow template.
- No CSE CSV button injection into this tab.

## Objects
- Template ID: `252`.
- CSE attaches a custom subclass to this template and injects two buttons at runtime:
  - `Export CSV`
  - `Import CSV`
- Existing native controls detected/used as layout anchors in this pane:
  - `Enable this type of data`
  - `Copy Objects From Other Region`
- Export/import operations target a list view auto-resolved by heuristics (column count, style, visibility, relation to anchor controls).

## Objects (more)
- Template ID: `309`.
- CSE attaches the same CSV subclass as Objects.
- Receives the same injected buttons (`Export CSV`, `Import CSV`).
- Uses strict schema handling for headers during import/export (see schema section below).

## Landscape
- Template ID: `3214`.
- Confirmed as a Region Editor subwindow template.
- No CSV overlay attached by CSE in this repo.

## Grass
- Template ID: `3220`.
- Confirmed as a Region Editor subwindow template.
- No CSV overlay attached by CSE in this repo.

## Sound
- Template ID: `3233`.
- CSE attaches the same CSV subclass and injects `Export CSV` / `Import CSV`.
- Template resolution logic can infer this tab from tab-caption text `Sound`.

## 3) Tab/button behavior (runtime mechanics)

### 3.1 Added buttons (Objects / Objects more / Sound)
On `WM_INITDIALOG`, CSE creates the two pushbuttons and positions them near the existing `Enable this type of data` control when present.

On `WM_COMMAND`:
- If `Export CSV`:
  1. Resolve active Region tab template.
  2. Locate best list view for that tab.
  3. Prompt: export selected region only (`Yes`) vs export all regions (`No`).
  4. Write CSV to `Data\\CSVExports\\Regions\\...`.
- If `Import CSV`:
  1. Resolve template and list view.
  2. Read CSV from the deterministic tab file path.
  3. Overwrite the list by deleting all rows and reinserting CSV content.

Default tab file names:
- Objects: `Data\\CSVExports\\Regions\\RegionEditor_Objects.csv`
- Objects (more): `Data\\CSVExports\\Regions\\RegionEditor_Objectsmore.csv`
- Sound: `Data\\CSVExports\\Regions\\RegionEditor_Sound.csv`

### 3.2 Region scope on export
For Objects tab export, CSE supports two scopes:
- current selected region only
- all regions (iterates region combo box entries and triggers `CBN_SELCHANGE` to refresh row data before serialization)

### 3.3 List-view resolution and robustness
CSE does not use fixed control IDs for tab list views. It scores candidates by:
- report style (`LVS_REPORT`)
- visibility/enabled state
- child relationship to dialog
- row/column count expectations (Objects ~= 8 columns, Objects more ~= 14)
- proximity to `Copy Objects From Other Region` button

This is why import/export survives modest UI layout drift.

## 4) CSV schema details by tab

### 4.1 Objects tab (`252`) — extended semantic schema
Export writes this explicit header:

`Region,Object,Density,MinSlope,MaxSlope,MinHeight,MaxHeight,Clustering,Radius,RadiusWrtParent,IsTree,IsHugeRock,Priority,Override`

Field capture strategy per row:
- Primary: read matching list-view columns.
- Fallback: select row and probe dialog controls by label text (`Min Slope`, `Is a Tree`, etc.) to recover values not present in visible columns.

Boolean normalization accepted/produced:
- truthy input tokens include `1`, `true`, `yes`, `y`, `checked`, `x`.

### 4.2 Objects (more) tab (`309`) — strict schema header map
Canonical headers:

`Region,Object,AngleVarianceXPlus,AngleVarianceXMinus,AngleVarianceYPlus,AngleVarianceYMinus,AngleVarianceZPlus,AngleVarianceZMinus,Sink,SinkVariance,SizeVariance,ConformToSlope,PaintVertices,PaintVerticesColor,PercentOfRadius`

### 4.3 Sound tab (`3233`) — generic list-view schema
- Export uses visible list headers/cells from the resolved list view.
- No tab-specific strict semantic schema is enforced like Objects/Objects-more.

### 4.4 Import validation
- For strict Objects/Objectsmore templates: CSV header must match expected schema naming.
- Objects tab additionally accepts the extended semantic header variant listed above.
- Import requires sufficient field count for the active list view.
- Import operation is destructive for list contents: `ListView_DeleteAllItems` before reinsert.

## 5) Region Asset CSVs (main menu, complementary to tab CSV)

CSE also adds top-level main menu entries under Import/Export for **Region Asset CSVs**:
- Export command ID: `IDC_MAINMENU_EXPORT_REGIONASSETCSV_ACTIVEPLUGIN` (`50179`)
- Import command ID: `IDC_MAINMENU_IMPORT_REGIONASSETCSV_ACTIVEPLUGIN` (`50180`)

Menu caption path includes:
- Export → Region Asset CSVs → Active Plugin + Parent Masters
- Import → Region Asset CSVs → Active Plugin + Parent Masters

### 5.1 Export behavior
- Requires active plugin.
- Builds allowed scope = active plugin + direct masters.
- Optional prompt to include/exclude `Oblivion.esm`.
- Exports supported region asset families: Tree, Flora, Grass, LandTexture, Rock (filtered Static), MiscSound.
- Can emit one combined CSV or split per type.
- Output root: `Data\\CSVExports\\Regions`.

### 5.2 Import behavior (current status)
- Current implementation is a **validation pipeline** with accounting counters.
- It validates schema/type tokens/form resolution/scope/type match and reports totals.
- It does **not** apply record mutation in this handler yet.

## 6) Safety/bugfix note relevant to tab operations

CSE installs a Region Editor hook (`RegionEditorCreateDataCopy`) that guards against null source data during data-copy flow, skipping the unsafe path and preventing a known crash condition when copying data on a brand new region record.

## 7) Practical editing workflow (for building/altering/reading values)

1. Open Region Editor and choose tab:
   - Objects / Objects (more) / Sound for CSE CSV roundtrip.
2. Use `Export CSV` from the tab.
3. Edit the generated file in external tools:
   - keep strict headers unchanged for Objects/Objectsmore;
   - preserve row width.
4. Re-open tab and click `Import CSV`.
5. Verify rows reloaded in list.
6. For cross-region bulk operations on Objects, use export scope = all regions.
7. For reference curation and scope checking of asset forms, use main menu Region Asset CSV export/import commands.

## 8) Known boundaries

- This repository exposes **template identity and CSE overlay mechanics** for all tabs.
- Deep native behavior for non-CSV tabs can now be captured through the Region Inspector pipeline below; semantic mapping remains heuristic and should be validated in-editor for final authoring docs.

## 9) Implemented Region Inspector pipeline (Phase 1-4)

CSE now includes a non-CSV Region tab inspector subclass for:
- General (`253`)
- Map (`256`)
- Weather (`251`)
- Landscape (`3214`)
- Grass (`3220`)

### Phase 1 — Static control map capture
On `WM_INITDIALOG`, the inspector recursively captures all child controls and writes:
- `Data\\CSVExports\\Regions\\Inspector\\RegionEditor_<Tab>_ControlMap_TabTemplateID.csv`

Captured columns include template ID/name, control ID, class, caption, style/ex-style (hex), bounds, parent control ID, tab-order ordinal, and nearest inferred static-label semantic.

### Phase 2 — Dynamic event map
For runtime `WM_COMMAND` events from controls, the inspector appends:
- `Data\\CSVExports\\Regions\\Inspector\\RegionEditor_<Tab>_EventLog.csv`

Each event row records timestamp, template/tab, source control ID/class/caption, notify code, semantic label, and before/after value snapshots.

### Phase 3 — Semantic mapping
A nearest-label heuristic associates controls with nearby `Static` captions to infer semantic names. This is used in both the control-map CSV and event-log rows.

### Phase 4 — Generated markdown documentation
On initialization (and on-demand), the inspector writes:
- `Data\\CSVExports\\Regions\\Inspector\\RegionEditor_<Tab>_ControlMap.md`

This file contains a per-tab table of controls and references the tab event log for behavior traces.

### Operator action
A `Dump Control Map` button is injected into inspected non-CSV tabs so users can force regeneration of CSV + markdown snapshots at any time without restarting the editor.
