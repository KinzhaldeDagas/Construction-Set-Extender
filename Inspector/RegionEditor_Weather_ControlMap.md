# Region Editor Weather tab - control inventory

Template ID: `251`

| TabOrder | ControlID | Class | Caption | Semantic Label | Bounds (L,T,W,H) | Style | ExStyle |
|---:|---:|---|---|---|---|---|---|
| 1 | 1062 | Button | Enable this type of data | Priority: | 11,13,140,16 | 50010003 | 00000004 |
| 2 | -1 | Static | Priority: | Priority: | 374,13,38,13 | 50020000 | 00000004 |
| 3 | 2028 | Edit | 50 | Priority: | 419,11,31,21 | 50010080 | 00000204 |
| 4 | 2029 | msctls_updown32 |  | Priority: | 448,11,18,21 | 50000036 | 00000004 |
| 5 | 2030 | Button | Override | Priority: | 510,13,66,16 | 50010003 | 00000004 |
| 6 | 2033 | SysListView32 |  |  | 11,41,182,174 | 5001020D | 00000204 |
| 7 | 0 | SysHeader32 |  | Weather Type | 13,43,178,24 | 500000C2 | 00000000 |
| 8 | 1020 | ComboBox | DefaultWeather | Weather Type | 222,63,149,21 | 50010303 | 00000004 |
| 9 | -1 | Static | Weather Type | Weather Type | 225,44,71,13 | 50020000 | 00000004 |
| 10 | 1235 | Button | Add |  | 387,63,47,21 | 50010000 | 00000004 |
| 11 | 9403 | Button | Dump Control Map | Priority: | 11,13,118,22 | 50010001 | 00000000 |

## Runtime behavior capture

Event data is appended to `Data\CSVExports\Regions\Inspector\RegionEditor_Weather_EventLog.csv`.
