UPD: 12 09 2026

This file is a manual, handwritten list from August this year, it should not be treated as anything definitive or up to date. 

For the new addon space UI and UX 

- done remove the addon column, mark addon space as "Add-on" type editor in the data category column 
- done but the bookmark ui is not yet. Bookmarks should add regardless of whether the panel is actually rendered at the moment. It just has to be in the list, that's all
- done remove the Add an addon entry as a whole
- done remove the header enum for panel context picking
- done remove the information icon from the header
- tooltip on hover that shows the full addon name and the path
- done mark bundled blender addons with an extra blender icon - or replace the plugin icon with the blender icon. The bundled addons, when enabled, should always be listed at the very top
- remove the curated list from the preferences, remove the "max addons shown" from it, move the "show bundled addons" to a different appropriate preferences category 
- done clicking the addon entry should show the user the first entry in the list, currently it wont show you anything until you click an actual, specific sub entry.
- is already in place. highlight the currently active panel and its owner in the list
- make sure alphabetical re-ordering still respects "blender bundled addons at the very top"

features: 
- add a gear icon that opens addon and extension preferences quickly potentially 
- think through the UX of opening the addon space in an area that is too narrow - open it with the sidebar collapsed? adding a widget of the collapse arrow visible when the side bar is open so that you can quickly collapse it? double clicking an addon panel focuses it and collapses the sidebar?

bugs: 

- fixed: after searching, all entries are left in a fully uncollapsed state and never recollapse to the state they were in pre-search. Update verification 1: the state remains after setting and resetting a search term BUT: the searched terms show the Addon name, but do refuse to show its sub entries, open or closed. Searching by editor name does show the entries though. And if you uncollapse an entry you searched for in the search list, it will remain uncollapsed once the search is cleared. UPD2: working. 

- fixed: bug QOL material panel and similar - when resizing it vertically via its internal gizmo, other panels do not make way for it until you collapse and re-open either them or the material panel. This only happens to us, the original panel in its space never has that. 

- fixed: extension name in the right side of the header regressed to showing its full path again instead of the extension name. this wasn't the case for a while, but it is again. 

thoughts: 
- the top edge of the inner panel being near flush with the header looks a bit ugly, but I don't know how much control we have over that.

