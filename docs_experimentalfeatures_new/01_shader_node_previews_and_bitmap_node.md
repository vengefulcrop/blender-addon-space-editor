# Shader Node Previews & Material Bitmap Node Architecture

## 1. Shader Node Previews Analysis

### 1.1 Codebase Locations
- **Implementation & Job Manager**: [`source/blender/editors/space_node/node_shader_preview.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/node_shader_preview.cc)
- **Header & Data Structures**: [`source/blender/editors/include/ED_node_preview.hh`](file:///d:/gember_fork/latest_pyareas/source/blender/editors/include/ED_node_preview.hh)
- **UI Drawing & Overlays**: [`source/blender/editors/space_node/node_draw.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/node_draw.cc#L2855)
- **Node Editing Intercepts**: [`source/blender/editors/space_node/node_edit.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/editors/space_node/node_edit.cc#L158)
- **RNA Gating**: [`source/blender/makesrna/intern/rna_userdef.cc`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesrna/intern/rna_userdef.cc#L7753)
- **DNA Preference Definition**: [`source/blender/makesdna/DNA_userdef_types.h`](file:///d:/gamedev/_blender_fork/latest_pyareas/source/blender/makesdna/DNA_userdef_types.h#L888) (`char use_shader_node_previews`)

### 1.2 Feature Gating Mechanism
The feature is gated under Blender's Experimental User Preferences system:
1. **DNA level**: `UserDef::use_shader_node_previews` boolean flag.
2. **RNA level**: `RNA_def_property(srna, "use_shader_node_previews", PROP_BOOLEAN, PROP_NONE)` under `UserDefExperimental`.
3. **Runtime check**: Evaluated via macro `USER_EXPERIMENTAL_TEST(&U, use_shader_node_previews)`. Checked prior to executing preview update cycles in `node_draw.cc` and `node_edit.cc`.

### 1.3 Implementation & Execution Pipeline
1. **Context & Hash Identification**:
   - `get_compute_context_hash_for_node_editor()` computes a unique hash (`ComputeContextHash`) based on the active node tree path in `SpaceNode`.
   - Node previews are cached per context hash in `SpaceNodeRuntime::tree_previews_per_context` using `NestedTreePreviews` structures.

2. **Job Scheduling & Preparation**:
   - `get_nested_previews()` calls `ensure_nodetree_previews()` when node overlay rendering is requested.
   - If a preview refresh is needed (e.g., node tree modified, resolution changed), a background `wmJob` of type `WM_JOB_TYPE_RENDER_PREVIEW` is launched via `WM_jobs_start()`.
   - `duplicate_material()` creates a local workspace copy of the material (`mat_copy`) inside the preview `Main` database (`G.pr_main`).

3. **Node Categorization & Socket Routing**:
   - Nodes are divided into **AOV nodes** (float, vector, color sockets) and **Shader nodes** (shader sockets).
   - **AOV Nodes**: Rendered simultaneously in a single base `ViewLayer`. Sockets are routed to temporary `SH_NODE_OUTPUT_AOV` nodes.
   - **Shader Nodes**: Rendered in individual `ViewLayer` instances. Sockets are routed into a temporary material surface output (`SH_NODE_OUTPUT_MATERIAL`).

4. **Off-Screen Rendering & Caching**:
   - The job invokes `RE_PreviewRender()` using the background preview scene (`preview_prepare_scene()`).
   - Render result images are retrieved via `get_image_from_viewlayer_and_pass()` and cached as `ImBuf` objects in `NestedTreePreviews::previews_map` keyed by `node.identifier`.

5. **UI Compositing & Drawing**:
   - During node drawing (`node_draw.cc`), `node_preview_acquire_ibuf()` fetches the cached `ImBuf` and binds it as a GPU texture drawn directly over the node body header.

---

## 2. Material Bitmap Node & Backend Feature Scope

### 2.1 Concept Overview
A material node (`ShaderNodeBakeToBitmap` / `Bake to Bitmap Node`) and backend engine that allows rendering any arbitrary combination of shader, vector, float, or color node inputs directly into a 2D bitmap (`ImBuf` / `Image` datablock), mirroring Substance Designer's Bake/Bitmap node system.

### 2.2 Optimal Architectural Design

#### A. Data Representation (`DNA` & `RNA`)
- **Node DNA**: Re-uses existing standard node structure (`bNode`).
- **Node Storage**: Stores an `Image *image` pointer, resolution parameters (`width`, `height`), bit depth (`8-bit`, `16-bit float`, `32-bit float`), and color management settings.
- **Node Sockets**:
  - **Inputs**: Flexible multi-input sockets (Color, Float, Vector, Shader) evaluated via AOV/surface routing.
  - **Outputs**: Output Image / Color / Vector / Float sockets providing baked texture outputs to downstream nodes.

#### B. Evaluation & Baking Backend Architecture
Instead of modifying shader compiler kernels or creating invasive low-level engine paths, the architecture leverages the existing **Off-Screen Preview & Bake Infrastructure**:

```
+------------------------+      +-------------------------------+      +-------------------------+
| ShaderNodeBakeToBitmap | ---> | ED_node_preview / Render Job  | ---> | GPU Offscreen / Cycles  |
| (Inputs: Node Subtree) |      | (AOV & Surface Pass Routing)  |      | Off-screen Evaluation   |
+------------------------+      +-------------------------------+      +-------------------------+
                                                                                    |
                                                                                    v
                                                                       +-------------------------+
                                                                       | Write to bke::Image /   |
                                                                       | ImBuf Pixel Buffer      |
                                                                       +-------------------------+
```

1. **Trigger Modes**:
   - **Manual Bake**: Triggered via operator or Python API call.
   - **Continuous/Auto Bake**: Dependency graph listener detects upstream node changes and tags the node preview job for background update.

2. **Evaluation Pathway**:
   - **GPU Off-screen Bake (Fast Path)**: Uses `GPUFrameBuffer` and `GPU_texture_read_format` to capture EEVEE/OpenGL viewport shader output into CPU/GPU `ImBuf` memory.
   - **Cycles Preview Bake (High Quality Path)**: Utilizes the `ED_node_preview` pattern (`RE_PreviewRender`) with temporary `SH_NODE_OUTPUT_AOV` routing to render arbitrary input combinations.

3. **Output Integration**:
   - Output pixels are updated directly in an `Image` datablock (`bNode::id`). Downstream nodes read this `Image` datablock seamlessly, eliminating re-evaluation overhead for heavy procedural subtrees.

---

## 3. High-Level Alternative Pathway (Zero DNA / Kernel Change Approach)

### Comparison of Technical Approaches

| Aspect | DNA / Shader Kernel Pathway (Low Level) | High-Level Infrastructure Re-use Pathway (Recommended) |
|---|---|---|
| **Architecture** | Modifies `DNA_node_types.h`, GPU codegen (`gpu_codegen.c`), and Cycles C++ kernel shaders. | Re-uses `bNode`, `Image` ID datablocks, `ED_node_preview`, and existing `GPUFrameBuffer` / `RE_PreviewRender` pipelines. |
| **Complexity** | High complexity; invasive edits across GPU shader generation, DNA headers, and render engine kernels. | Low to moderate complexity; contained entirely within node editor utilities and asset/render backend. |
| **Stability Risk** | High risk of breaking shader code generation across GLSL and Cycles SVM compilers. | Zero risk to core shader compiler or kernel runtime; operates at editor/evaluation layer. |
| **Functionality** | Identical functionality. | Identical functionality, with built-in support for asynchronous background execution. |

### Technical Justification
By utilizing existing `Image` datablock storage and the proven off-screen rendering mechanics of `node_shader_preview.cc`, the feature achieves full parity with Substance Designer baking nodes without requiring any changes to shader compiler code generation or kernel types.

---

## 4. Python API Design for Add-ons

### 4.1 RNA Interface (`bpy.types.ShaderNodeBakeToBitmap`)
```python
class ShaderNodeBakeToBitmap(bpy.types.ShaderNode):
    # Core Properties
    image: bpy.types.Image                   # Output baked image datablock
    resolution: bpy.props.IntVectorProperty # (width, height), default (1024, 1024)
    color_space: bpy.props.StringProperty   # Color space profile (sRGB, Non-Color, etc.)
    auto_bake: bpy.props.BoolProperty       # Auto update when upstream nodes change
    is_baking: bpy.props.BoolProperty       # Read-only state indicating active baking

    # Core Methods
    def bake(self, execution_context='ASYNC') -> bool:
        """Triggers bitmap baking. Supports 'ASYNC' (background job) or 'SYNC'."""
        ...

    def cancel_bake(self) -> None:
        """Cancels an active baking job."""
        ...

    def get_pixel_buffer(self) -> numpy.ndarray / memoryview:
        """Direct high-performance buffer access to baked bitmap pixels."""
        ...
```

### 4.2 Add-on Integration Pattern
```python
import bpy

# Add node and trigger programmatic bake
mat = bpy.data.materials["Material"]
nodes = mat.node_tree.nodes

bake_node = nodes.new(type="ShaderNodeBakeToBitmap")
bake_node.resolution = (2048, 2048)
bake_node.auto_bake = True

# Connect procedural texture into bake node input
voronoi = nodes.new(type="ShaderNodeTexVoronoi")
mat.node_tree.links.new(voronoi.outputs["Color"], bake_node.inputs[0])

# Trigger asynchronous bake from python add-on
bake_node.bake(execution_context='ASYNC')
```
