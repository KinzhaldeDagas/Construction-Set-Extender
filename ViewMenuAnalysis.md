# View Menu Codebase Analysis

## Scope
This document analyzes the main **`View`** dropdown declared in `Construction Set Extender.rc` and tracks each entry to its behavior in extender code (`MainWindowOverrides.cpp`) or to base-editor handling.

## Source of truth for menu layout
The `View` popup and all visible labels are declared in the resource file. IDs are either vanilla CS command IDs (e.g., `40199`) or CSE command IDs (e.g., `IDC_MAINMENU_*`).

- Menu declaration: `Construction Set Extender.rc` (`POPUP "&View"` block).
- Extender command state sync (`checked`/`disabled` at open time): `MainWindowMenuInitSubclassProc` in `MainWindowOverrides.cpp`.
- Extender command execution (`WM_COMMAND`): `MainWindowMenuSelectSubclassProc` in `MainWindowOverrides.cpp`.

## Runtime behavior by option

| View menu label | Command ID | Execution path | Behavior summary |
|---|---:|---|---|
| Custom Color Theme Mode | `IDC_MAINMENU_CUSTOMCOLORTHEMEMODE` | CSE handler | Toggles UI color themer enable/disable. |
| Toolbar | `40102` | Vanilla CS | Standard toolbar visibility toggle (not overridden by CSE). |
| Statusbar | `40026` | Vanilla CS | Standard status bar visibility toggle (not overridden by CSE). |
| Render Window | `40423` | Vanilla CS | Opens/focuses render window. |
| Object Window | `40199` | Vanilla CS | Opens/focuses object window. |
| Cell View Window | `40200` | Vanilla CS | Opens/focuses cell view window. |
| Preview Window | `40121` | Mostly vanilla; gated by CSE | When multiple preview windows are enabled, CSE blocks default open and shows guidance message; otherwise passes through to vanilla. |
| Multiple Preview Windows | `IDC_MAINMENU_MULTIPLEPREVIEWWINDOWS` | CSE handler | Toggles imposter preview window system and persists setting. |
| Spawn Extra Object Window | `IDC_MAINMENU_SPAWNEXTRAOBJECTWINDOW` | CSE handler | Spawns an additional object window. |
| Memory Usage Window | `40439` | Vanilla CS | Vanilla window/tool command (no CSE override case). |
| Open Windows... | `40455` | Vanilla CS | Vanilla open windows dialog command (no CSE override case). |
| Global Clipboard Contents... | `IDC_MAINMENU_GLOBALCLIPBOARDCONTENTS` | CSE handler | Displays shared/global clipboard contents. |
| Console Window | `IDC_MAINMENU_CONSOLE` | CSE handler | Toggles CSE console visibility. |
| Auxiliary Viewport Window | `IDC_MAINMENU_AUXVIEWPORT` | CSE handler | Toggles auxiliary viewport visibility. |
| Tag Browser... | `IDC_MAINMENU_TAGBROWSER` | CSE handler | Opens Tag Browser tool dialog. |
| Centralized Use Info Listing... | `IDC_MAINMENU_USEINFOLISTING` | CSE handler | Opens Use Info Listing dialog. |
| Hide Unmodified Records | `IDC_MAINMENU_HIDEUNMODIFIEDFORMS` | CSE handler | Toggles object/form list visibility filter for unmodified forms. |
| Hide Deleted Records | `IDC_MAINMENU_HIDEDELETEDFORMS` | CSE handler | Toggles object/form list visibility filter for deleted forms. |
| Sort Active Records First | `IDC_MAINMENU_SORTACTIVEFORMSFIRST` | CSE handler | Toggles active-plugin-first sorting behavior. |
| Colorize Active Records | `IDC_MAINMENU_COLORIZEACTIVEFORMS` | CSE handler | Toggles colorization of active records in list UI. |
| Colorize Record Overrides | `IDC_MAINMENU_COLORIZEFORMOVERRIDES` | CSE handler | Toggles colorization of override records in list UI. |
| Markers | `40030` | Vanilla CS | Render debug/overlay visibility toggle. |
| Light Radius | `40031` | Vanilla CS | Render visualization toggle. |
| Wireframe | `40118` | Vanilla CS | Render mode toggle. |
| Bright Light | `40206` | Vanilla CS | Render lighting toggle. |
| Sky | `40223` | Vanilla CS | Sky rendering toggle. |
| Solid Subspaces | `40482` | Vanilla CS | Render geometry visualization toggle. |
| Collision Geometry | `40189` | Vanilla CS | Collision visualization toggle. |
| Leaves | `290` | Vanilla CS | Flora sub-feature visibility toggle. |
| Trees | `40480` | Vanilla CS | Tree rendering visibility toggle. |
| Parent-Child Indicator | `IDC_MAINMENU_PARENTCHILDINDICATORS` | CSE handler | Toggles renderer parent-child visual indicator; forces redraw. |
| Path Grid Linked Ref Indicator | `IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORS` | CSE handler | Toggles linked-ref indicator visualization; forces redraw. |
| Path Grid Linked Ref Indicator Visibility... / Hide Path Grid Point Bounding Box | `IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDEBOUNDINGBOX` | CSE handler | Toggles hide flag for point bounding box sub-element. |
| Path Grid Linked Ref Indicator Visibility... / Hide Linked Ref Node | `IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINKEDREFNODE` | CSE handler | Toggles hide flag for linked-ref node sub-element. |
| Path Grid Linked Ref Indicator Visibility... / Hide Line Connector | `IDC_MAINMENU_PATHGRIDLINKEDREFINDICATORSETTINGS_HIDELINECONNECTOR` | CSE handler | Toggles hide flag for connector line sub-element. |
| Initially Disabled References | `IDC_MAINMENU_INITIALLYDISABLEDREFERENCES` | CSE handler | Toggles visibility of initially disabled refs in render window; redraws. |
| Initially Disabled References' Children | `IDC_MAINMENU_CHILDREFERENCESOFTHEDISABLED` | CSE handler | Toggles visibility of children of initially disabled refs; redraws. |
| Isometric | `40032` | Vanilla CS | Camera/view orientation command. |
| Top | `40033` | Vanilla CS | Camera/view orientation command. |

## Dynamic state handling (`checked` / `enabled`)
When the user opens a popup menu, CSE updates checkmarks for CSE-managed items in `WM_INITMENUPOPUP`:

- Check state updated for: console, aux viewport, filters/sorting/colorization, multiple preview windows, parent-child indicators, path-grid indicator + its sub-flags, initially disabled refs toggles, and custom color theme mode.
- Enable state updated for non-View commands too (e.g., global undo/redo), via same shared routine.
- Vanilla `View` items (e.g., `Toolbar`, `Sky`) are not explicitly synchronized in this CSE routine, so their checked state is managed by vanilla CS.

## Existing seams relevant to a future View-menu reorganization

1. **Best low-risk path:** reorder/group labels directly in `Construction Set Extender.rc` while preserving command IDs.
2. **No behavioral coupling to visual order:** CSE command handling is ID-driven in `WM_COMMAND`; changing order/group separators in RC does not require switch-case rewrites.
3. **Submenu precedent already exists:** path-grid indicator visibility submenu demonstrates nested grouping pattern that can be reused for broader functional categories.
4. **Vanilla command IDs are safe to move within View popup:** they continue routing through vanilla handlers as long as IDs stay unchanged.

## Proposed category taxonomy for reorganization

A practical category set that matches existing functionality:

1. **Window Visibility**
   - Toolbar, Statusbar, Render/Object/Cell View/Preview, Open Windows, Memory Usage, Console, Auxiliary Viewport, Spawn Extra Object Window, Multiple Preview Windows.
2. **Data & List Presentation**
   - Hide Unmodified Records, Hide Deleted Records, Sort Active Records First, Colorize Active Records, Colorize Record Overrides, Tag Browser, Use Info Listing, Global Clipboard Contents.
3. **Render Display Toggles**
   - Markers, Light Radius, Wireframe, Bright Light, Sky, Solid Subspaces, Collision Geometry, Leaves, Trees.
4. **Render Relationship Overlays**
   - Parent-Child Indicator, Path Grid Linked Ref Indicator, Path Grid Linked Ref Indicator Visibility submenu.
5. **Reference Visibility Rules**
   - Initially Disabled References, Initially Disabled References' Children.
6. **Camera Orientation**
   - Isometric, Top.
7. **Theme / Appearance**
   - Custom Color Theme Mode.

This preserves every current function but makes the View menu intent-driven and easier to scan.
