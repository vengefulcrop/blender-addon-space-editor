# SPDX-License-Identifier: GPL-2.0-or-later
"""
Regression test for defect 14: a crash after "Swap Areas".

`ED_area_swapspace()` exchanges the space data of two areas. It does not exchange
`ScrArea::context_delegate_spacetype`. An Add-on editor leaves that field behind. The
Properties editor that lands in the same area then resolves its space through the
delegate area, `CTX_wm_space_properties()` returns null, and `buttons_context_compute()`
reads through the null pointer.

The test needs three areas. Two of them share an edge and become the Add-on editor and
the Properties editor. The third becomes a Shader Editor, which is the editor Node
Wrangler borrows context from. The test then calls `screen.area_swap` on the shared
edge. Without the fix Blender crashes on the next redraw. With the fix both areas report
`context_delegate_spacetype` as 'EMPTY' right after the swap.

Run from Blender's Text Editor (Alt-P) or the Python Console:

    exec(open("<repo>/tests/pyareas/pyareas_area_swap_delegate.py").read())
"""

import addon_utils
import bpy

ADDON = "node_wrangler"
KEEP = {"CONSOLE", "TEXT_EDITOR"}


def ensure_material():
    """The Node Wrangler poll needs a node tree, which needs a material."""
    obj = bpy.context.view_layer.objects.active
    if obj is None:
        obj = next((o for o in bpy.context.view_layer.objects), None)
        if obj is None:
            print("[swap] no object in the scene")
            return False
        bpy.context.view_layer.objects.active = obj

    if not obj.data or not hasattr(obj.data, "materials"):
        print(f"[swap] {obj.name!r} cannot hold materials")
        return False

    if not obj.material_slots:
        mat = bpy.data.materials.new("AddonEditorSwapTest")
        mat.use_nodes = True
        obj.data.materials.append(mat)
    return True


def shared_edge(a, b):
    """Return a point on the edge between two areas, or None when they do not touch."""
    ax1, ay1, ax2, ay2 = a.x, a.y, a.x + a.width, a.y + a.height
    bx1, by1, bx2, by2 = b.x, b.y, b.x + b.width, b.y + b.height

    for x in (ax1, ax2):
        if abs(x - bx1) <= 2 or abs(x - bx2) <= 2:
            lo, hi = max(ay1, by1), min(ay2, by2)
            if hi - lo > 20:
                return (x, (lo + hi) // 2)

    for y in (ay1, ay2):
        if abs(y - by1) <= 2 or abs(y - by2) <= 2:
            lo, hi = max(ax1, bx1), min(ax2, bx2)
            if hi - lo > 20:
                return ((lo + hi) // 2, y)

    return None


def find_pair(screen):
    """Find two adjacent areas that the test may take over."""
    areas = [a for a in screen.areas if a.type not in KEEP]
    for i, a in enumerate(areas):
        for b in areas[i + 1:]:
            point = shared_edge(a, b)
            if point is not None:
                return a, b, point
    return None, None, None


def main():
    if not addon_utils.check(ADDON)[1]:
        addon_utils.enable(ADDON, default_set=True)

    screen = bpy.context.screen
    addon_area, props_area, point = find_pair(screen)
    if addon_area is None:
        print("[swap] need two adjacent areas, split the window and rerun")
        return

    if not ensure_material():
        return

    spare = next((a for a in screen.areas
                  if a.type not in KEEP and a not in (addon_area, props_area)), None)
    if spare is None:
        print("[swap] need a third area for the Shader Editor, split the window and rerun")
        return

    spare.type = "NODE_EDITOR"
    spare.spaces.active.tree_type = "ShaderNodeTree"
    spare.spaces.active.shader_type = "OBJECT"

    addon_area.type = "ADDON"
    addon_area.spaces.active.addon_id = ADDON
    props_area.type = "PROPERTIES"

    # The delegate field is only set during the Add-on editor layout pass.
    bpy.ops.wm.redraw_timer(type="DRAW_WIN_SWAP", iterations=1)
    print(f"[swap] before: addon={addon_area.context_delegate_spacetype!r} "
          f"props={props_area.context_delegate_spacetype!r}")

    delegate = addon_area.context_delegate_spacetype
    if delegate in {"EMPTY", "PROPERTIES"}:
        print(f"[swap] delegate is {delegate!r}, the test cannot reproduce the crash")
        return

    bpy.ops.screen.area_swap(cursor=point)

    after = [a.context_delegate_spacetype for a in (addon_area, props_area)]
    print(f"[swap] after: {after[0]!r} {after[1]!r}")

    # A redraw of the swapped areas is where the unfixed build crashes.
    bpy.ops.wm.redraw_timer(type="DRAW_WIN_SWAP", iterations=1)

    if any(t != "EMPTY" for t in after):
        print("[swap] FAIL: a stale delegate survived the swap")
    else:
        print("[swap] PASS: both areas cleared the delegate, and the redraw held")


main()
