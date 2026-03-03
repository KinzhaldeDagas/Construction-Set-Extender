# Inspector CSV Research Notes

## Dataset inventory

The `Inspector` folder currently contains **11 CSV files** split into two groups:

- **5 control maps** (`*ControlMap*`): describe control metadata such as class, position, style, and semantic label.
- **6 event logs** (`*EventLog*`): capture runtime notifications (`NotifyCode`) with before/after values.

Tabs represented: `General`, `Map`, `Weather`, `Landscape`, `Grass`, `Sound`.

## Schema summary

### Control map CSVs
Common columns:

`TabTemplateID, TabName, ControlID, ClassName, Caption, StyleHex, ExStyleHex, Left, Top, Width, Height, ParentControlID, TabOrder, SemanticLabel`

### Event log CSVs
Common columns:

`Timestamp, TemplateID, TabName, ControlID, ClassName, Caption, SemanticLabel, NotifyCode, BeforeValue, AfterValue`

## Per-file profile

| File | Data rows | Notes |
|---|---:|---|
| `RegionEditor_General_ControlMap_TabTemplateID_253.csv` | 14 | Mix of `Edit`/`Button`/`Static`; 11 unique control IDs. |
| `RegionEditor_General_EventLog.csv` | 12 | All `Edit` events; notify mix `1024`, `768`, `256`, `512`; ~1.5s span. |
| `RegionEditor_Grass_ControlMap_TabTemplateID_3220.csv` | 28 | Richest control map: `Edit`, `Static`, `Button`, plus tree view + spin control. |
| `RegionEditor_Grass_EventLog.csv` | 44 | High-volume `Edit` activity; only `1024`/`768` notifications. |
| `RegionEditor_Landscape_ControlMap_TabTemplateID_3214.csv` | 6 | Compact map with spin edit controls. |
| `RegionEditor_Landscape_EventLog.csv` | 5 | Includes one `Button` event (`NotifyCode=0`) plus edit notifications. |
| `RegionEditor_Map_ControlMap_TabTemplateID_256.csv` | 8 | Minimal map with spin edit pair. |
| `RegionEditor_Map_EventLog.csv` | 10 | `Edit` notifications only (`1024`/`768`). |
| `RegionEditor_Sound_EventLog.csv` | 42 | Similar control-ID pattern to Grass events; no matching Sound control map file present. |
| `RegionEditor_Weather_ControlMap_TabTemplateID_251.csv` | 11 | Includes list-view/header/combobox controls in addition to button/edit/static. |
| `RegionEditor_Weather_EventLog.csv` | 4 | Smallest event log; single control ID (`2028`). |

## Cross-file findings

1. **Control-map coverage is incomplete for event logs**
   - `Sound` has events but no `RegionEditor_Sound_ControlMap_*.csv` file.
   - `Map` events include control `2028` not listed in the map control map.
   - `Landscape` events include controls `2024` and `2025` not listed in the landscape control map.

2. **Grass and Sound event logs appear structurally parallel**
   - Both contain 11 matching control IDs (`2128` through `2148`, sparse set).
   - Both are entirely `Edit` events with a strict `1024`/`768` notify-code pairing pattern.

3. **Notify code pattern is mostly two-phase edit notifications**
   - Most logs alternate between `1024` then `768` for the same control, suggesting begin/end or change/commit style signaling.
   - `General` additionally contains `256` and `512`, and `Landscape` includes a single `0` from a button control.

4. **Static pseudo-controls are represented with `ControlID = -1` in control maps**
   - Seen across multiple tabs (e.g., label-like controls), consistent with non-interactive visual elements.

## Suggested next steps

- Export or regenerate missing control maps (especially `Sound`) to enable full control-to-event reconciliation.
- Confirm semantics of `NotifyCode` values (`1024`, `768`, `512`, `256`, `0`) against the UI framework constants.
- If these files feed tooling, add a validation step that flags event log control IDs absent from the corresponding control map.
