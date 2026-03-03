# Inspector CSV Research

## Source
This research targets the upstream Inspector folder at:
- https://github.com/KinzhaldeDagas/Construction-Set-Extender/tree/master/Inspector

I cloned the upstream repository to `/workspace/cse_upstream` and analyzed CSV files under `/workspace/cse_upstream/Inspector`.

## Inventory
The Inspector folder contains **10 CSV files** in two groups:

### Control-map CSVs (5)
- `RegionEditor_General_ControlMap_TabTemplateID_253.csv`
- `RegionEditor_Grass_ControlMap_TabTemplateID_3220.csv`
- `RegionEditor_Landscape_ControlMap_TabTemplateID_3214.csv`
- `RegionEditor_Map_ControlMap_TabTemplateID_256.csv`
- `RegionEditor_Weather_ControlMap_TabTemplateID_251.csv`

Schema (14 columns):
`TabTemplateID, TabName, ControlID, ClassName, Caption, StyleHex, ExStyleHex, Left, Top, Width, Height, ParentControlID, TabOrder, SemanticLabel`

### Event-log CSVs (5)
- `RegionEditor_Grass_EventLog.csv`
- `RegionEditor_Landscape_EventLog.csv`
- `RegionEditor_Map_EventLog.csv`
- `RegionEditor_Unknown_EventLog.csv`
- `RegionEditor_Weather_EventLog.csv`

Schema (10 columns):
`Timestamp, TemplateID, TabName, ControlID, ClassName, Caption, SemanticLabel, NotifyCode, BeforeValue, AfterValue`

## Quantitative findings

### Control-map row counts
- General (Template 253): 14 rows
- Grass (Template 3220): 28 rows
- Landscape (Template 3214): 6 rows
- Map (Template 256): 8 rows
- Weather (Template 251): 11 rows

Notable class usage:
- `Grass` is the most complex tab (includes `SysTreeView32` and many labeled edit/static controls).
- `Weather` includes list controls (`SysListView32`, `SysHeader32`) and a `ComboBox`.
- `Map`, `Landscape`, and `Weather` each include an `msctls_updown32` spin control for priority-like numeric editing.

### Event-log row counts
- Grass: 44 rows
- Landscape: 4 rows
- Map: 10 rows
- Unknown: 42 rows
- Weather: 9 rows

Notify code behavior:
- Most events are split evenly between notify codes `1024` and `768`.
- Weather has one extra event with notify code `0`.

## Behavioral observations from event logs
- Typical captured edits include `Priority:` changes (e.g., blank -> `50`).
- Map events also capture `Map Name` changing from blank -> `Default Region Name`.
- Unknown template events are high-volume but currently lack semantic labels/decoded context.

## Practical takeaways
1. The ControlMap CSVs provide stable UI metadata (control IDs, class names, geometry, styles, semantic labels) for tab-specific mappings.
2. The EventLog CSVs capture runtime interaction traces and value transitions suitable for reverse-engineering control semantics.
3. `Unknown` event logs should be correlated against future control maps/template IDs to resolve currently unmapped controls.

## Commands used
- `git clone --depth 1 https://github.com/KinzhaldeDagas/Construction-Set-Extender.git /workspace/cse_upstream`
- `find /workspace/cse_upstream/Inspector -type f`
- Python CSV summaries over `/workspace/cse_upstream/Inspector/*.csv` (row counts, schemas, class distribution, notify-code distribution, and value-change detection)
