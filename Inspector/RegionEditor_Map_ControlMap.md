# Region Editor Map tab - control inventory

Template ID: `256`

| TabOrder | ControlID | Class | Caption | Semantic Label | Bounds (L,T,W,H) | Style | ExStyle |
|---:|---:|---|---|---|---|---|---|
| 1 | 2023 | Button | Enable this type of data | Priority: | 11,13,140,16 | 50010003 | 00000004 |
| 2 | -1 | Static | Priority: | Priority: | 374,13,38,13 | 50020000 | 00000004 |
| 3 | 2025 | Edit | 50 | Priority: | 419,11,31,20 | 50010080 | 00000204 |
| 4 | 2026 | msctls_updown32 |  | Priority: | 448,11,18,20 | 50000036 | 00000004 |
| 5 | 2027 | Button | Override | Priority: | 510,13,66,16 | 50010003 | 00000004 |
| 6 | -1 | Static | Map Name | Map Name | 11,57,51,13 | 50020000 | 00000004 |
| 7 | 2024 | Edit | Default Region Name | Map Name | 86,54,303,24 | 50010080 | 00000204 |
| 8 | 9403 | Button | Dump Control Map | Priority: | 11,13,118,22 | 50010001 | 00000000 |

## Runtime behavior capture

Event data is appended to `Data\CSVExports\Regions\Inspector\RegionEditor_Map_EventLog.csv`.
