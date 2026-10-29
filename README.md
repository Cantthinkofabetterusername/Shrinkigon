# Shrinkigon

**English** | [Español](README.es.md)

Shrinkigon is a macOS desktop tool for simplifying 3D meshes. It loads a model, reduces its triangle count with an edge-collapse algorithm based on Quadric Error Metrics, shows before/after statistics, previews the result in an interactive 3D viewport and exports the optimized model. It also includes an optional cloud-processing extension called **Speedigon**.

It is written in C++17 with Dear ImGui, OpenGL 2.1, Assimp and OpenMesh, and it is aimed at developers who want a small, readable mesh-decimation tool.

> ## Speedigon is a simulation, not a real purchase
> The "Buy Now" button and the "$5 USD" price shown in the app are **placeholders**. No payment is processed and no payment provider is integrated. Pressing the button only sets a `speedigon = true` flag in the user's database record, so the feature can be tested end to end. Do not treat this project as a commercial product or as a payment flow example.

## Table of contents

- [Features](#features)
- [Requirements](#requirements)
- [Building](#building)
- [Configuration](#configuration)
- [Usage](#usage)
- [Speedigon and the cloud backend](#speedigon-and-the-cloud-backend)
- [Algorithms](#algorithms)
- [Known limitations](#known-limitations)
- [Project layout](#project-layout)
- [Security notes](#security-notes)
- [Third-party software](#third-party-software)
- [License](#license)

## Features

- Loads `.gltf`, `.glb`, `.obj` and `.fbx` models through Assimp, including their textures.
- Quadric-error edge-collapse decimation with constraints for boundaries, hard edges, normal flips and UV mapping.
- Interactive 3D preview (orbit, pan, zoom) over a baseplate, similar to the one in game-engine editors.
- Before/after statistics: vertices, edges and memory.
- A "Remesh Model" button that restores the model as it was before the optimization.
- Exports the optimized model next to the original folder.
- User interface in English, Spanish, German and Italian (English by default).
- Optional accounts and the **Speedigon** extension, which runs the optimization on a cloud CPU server and removes the local triangle limit.
- A neon theme that is applied while Speedigon is active.

## Requirements

- macOS. The project is macOS-only: it uses the system OpenGL framework and `osascript` for the folder picker. Build it on the architecture you want to run it on (Apple Silicon or Intel).
- Xcode Command Line Tools (`xcode-select --install`).
- CMake and a C++17 compiler.
- Libraries: GLFW, FreeType, Assimp, OpenMesh and libcurl. Dear ImGui (with the GLFW backend and the OpenGL 2.1 renderer) and `stb_image.h` are included in this repository.

With Homebrew:

```bash
brew install cmake glfw freetype assimp open-mesh curl
```

## Building

```bash
git clone https://github.com/<your-user>/shrinkigon.git
cd shrinkigon
cp cloud_config.example.h cloud_config.h
cmake -S . -B build
cmake --build build
```

The application looks for its fonts in a `fonts/` folder **relative to the working directory**, so run the executable from the repository root:

```bash
./build/<executable-name>
```

## Configuration

Accounts and Speedigon need a backend that you deploy yourself (see [Speedigon and the cloud backend](#speedigon-and-the-cloud-backend)). Its addresses live in `cloud_config.h`, which is intentionally not committed:

| Constant | Meaning |
| --- | --- |
| `kCloudApiBaseUrl` | Base URL of the HTTP API that handles accounts (register, login, profile, language, purchase). |
| `kSpeedigonApiBaseUrl` | Base URL of the optimization server (`/optimize`). |
| `kCloudAuthTimeoutSeconds` | Timeout for account requests. |
| `kCloudOptimizeTimeoutSeconds` | Timeout for an optimization request. |

While a URL still contains `REPLACE_ME`, the cloud features report "not configured" and everything else keeps working locally. This repository does not give access to any hosted backend.

User settings (language, username and session token) are stored in `~/.shrinkigon_settings`.

## Usage

1. Press **Upload Asset** and choose the folder that contains the model. Shrinkigon uses the first `.gltf`, `.glb`, `.obj` or `.fbx` file it finds in that folder. The model appears in the viewport and the **Asset Information** panel shows its vertices, edges and memory.
2. Set the optimization parameters (table below) and press **Optimize Model**.
3. When it finishes, the panel shows the optimized numbers and the reduction. You can inspect the result in the viewport.
4. Press **Remesh Model** to go back to the model as it was before the optimization and try other parameters.
5. Press **Save Optimized Model** to export it.

**Viewport controls:** left-drag to orbit, right-drag or middle-drag to pan, mouse wheel to zoom.

### Parameters

| Parameter | Effect |
| --- | --- |
| Reduction Percentage | Percentage of triangles to remove (0 to 100). This is what defines the target of the algorithm. |
| Prevent Normal Flipping | Rejects collapses that rotate any affected face by more than about 72.5 degrees. |
| Preserve Hard Edges | Does not collapse edges whose two faces form an angle above the threshold. |
| Hard Edge Angle | Threshold for the previous option, in degrees (default 45). |
| Lock Boundary Edges | Keeps vertices on open boundaries fixed. |
| Preserve UV Mapping | Keeps UV seam vertices fixed and rejects collapses that distort the texture mapping. |
| Target Triangle Count | Present in the interface but **not used by the algorithm yet**. |
| Maximum Deformation Error | Present in the interface but **not used by the algorithm yet**. |

### Limits

- Without Speedigon, models above **100,000 triangles** cannot be optimized. A window explains it and points to Speedigon.
- An optimization is stopped after **60 seconds**. The model is left untouched and a message explains what happened.
- With Speedigon active there is no triangle limit, but the server stops each job after 50 seconds.

### Saving

The optimized model is written to a folder named `<input folder>_shrinkigonized`, next to the input folder. If a folder with that name already exists, it is **deleted and recreated**. The model is exported in its original format when Assimp supports it, and as `.obj` otherwise.

## Speedigon and the cloud backend

Speedigon is an optional extension. When it is active, the optimization is sent to a server that runs the same algorithm on a CPU instance, and the local triangle limit is removed. If the server cannot be reached or fails, the app optimizes locally and tells you.

To use it, a user has to create an account, press **Buy Now** (simulated, see the notice at the top) and then press **Activate Speedigon** in Settings.

### Architecture

```
Shrinkigon (C++, libcurl)
   |-- accounts ---> HTTP API (API Gateway) --> Lambda (Node.js) --> DynamoDB
   |-- /optimize --> optimization server (C++, cpp-httplib, EC2)
                        |-- validates the token with GET /me on the HTTP API
```

- **Accounts.** A DynamoDB table keyed by `username` stores `passwordHash`, `language` and `speedigon`. Passwords are hashed with scrypt (random 16-byte salt). Sessions use a token of the form `base64url(payload).base64url(HMAC-SHA256)` that expires after 30 days. Usernames are lowercase, 3 to 32 characters (`A-Z a-z 0-9 . _ -`); passwords are 8 to 128 characters.
- **Optimization server.** `server/speedigon_server.cpp` is a small HTTP server on port 8080. For each request it checks the bearer token, decodes the meshes, runs the decimation and returns the result. It processes one job at a time and answers `busy` if another one is running.

### HTTP API

| Method and path | Served by | Purpose |
| --- | --- | --- |
| `POST /register` | Lambda | Create an account. |
| `POST /login` | Lambda | Log in and get a token. |
| `GET /me` | Lambda | Profile and `speedigon` flag. |
| `POST /language` | Lambda | Save the language preference. |
| `POST /purchase` | Lambda | **Simulated purchase:** sets `speedigon = true` for the authenticated user. |
| `POST /optimize` | EC2 server | Optimize meshes (requires `speedigon = true`). The Lambda has a stub for this route that answers 501. |

Every response is JSON with an `ok` field and, on failure, a `code` such as `unauthorized`, `invalid_credentials`, `username_taken`, `not_owned`, `busy` or `timeout`.

### Optimization wire format

The request body is `{"config": {...}, "payload": "<base64>"}`. The payload is binary, little-endian: a `uint32` mesh count, then for each mesh a `uint32` vertex count, a `uint32` index count, `vertexCount x 8` `float32` values (position xyz, normal xyz, u, v) and `indexCount` `uint32` indices. The response is `{"ok": true, "payload": "<base64>"}` with the same layout. Textures and materials never leave the computer: the client keeps them and only replaces the geometry.

### Deploying your own backend

1. **DynamoDB:** create a table with partition key `username` (String).
2. **Lambda:** create a Node.js function with the code in `aws/lambda/index.mjs`. Set the environment variables `TABLE_NAME` and `TOKEN_SECRET` (a long random string, for example `openssl rand -hex 32`). Give its role `dynamodb:GetItem`, `dynamodb:PutItem` and `dynamodb:UpdateItem` on the table.
3. **API Gateway:** create an HTTP API with routes `POST /register`, `POST /login`, `GET /me`, `POST /language` and `POST /purchase`, all integrated with the Lambda. If you attach integrations from the CLI, also grant API Gateway permission to invoke the function (`aws lambda add-permission`).
4. **Optimization server:** on a Linux instance install `g++`, `libopenmesh-dev`, `libcpp-httplib-dev` and `libcurl4-openssl-dev`, then build:

   ```bash
   g++ -O3 -std=c++17 -pthread server/speedigon_server.cpp mesh_codec.cpp mesh_chunking.cpp optimization_algorithms.cpp -I. -lOpenMeshCore -lcurl -o speedigon_server
   ```

   Run it with `SPEEDIGON_AUTH_URL` set to the HTTP API base URL (see `server/speedigon.service` for a systemd unit) and open port 8080 in the security group.
5. Put both URLs in `cloud_config.h` and rebuild the app.

## Algorithms

All the geometry processing lives in `optimization_algorithms.cpp`. This section describes what it does, step by step. The approach follows the quadric-error-metrics method of Garland and Heckbert (1997) with extra constraints for textured models.

### 1. Import

Models are read with Assimp using these post-processing flags: triangulate, generate smooth normals, join identical vertices, flip UVs and pre-transform vertices (the node hierarchy is flattened and every transform is baked into the vertices, which typically leaves one mesh per material). Each Assimp mesh becomes a `MeshData`: a vertex array (position, normal, u, v), a triangle index list and its material information (texture or diffuse color). Textures are loaded with `stb_image`.

The decimation runs **independently on every mesh**; edges are never collapsed across two meshes.

### 2. Conversion to a half-edge mesh

Every `MeshData` is converted to an OpenMesh triangle mesh (`TriMesh_ArrayKernelT`) with half-edge texture coordinates:

1. **Welding.** Positions are quantized with `epsilon = max(1e-6 x bounding-box diagonal, 1e-9)` and vertices that fall in the same cell become one vertex. Models usually store a vertex several times (once per UV or normal discontinuity); welding recovers the real connectivity.
2. **Faces.** A face whose corners collapse into fewer than three distinct vertices is skipped. If OpenMesh refuses a face because it would create a non-manifold configuration ("complex vertex"), its three vertices are duplicated and the face is added again.
3. **UVs.** Texture coordinates are stored **per face corner** (on half-edges), not per vertex. That is what allows UV seams to survive welding.

### 3. Target size

The percentage is clamped to the range 0 to 100 and the target is

```
target = max(1, floor(originalTriangles x (1 - percentage / 100)))
```

If the target is not smaller than the original count, the mesh is returned unchanged. Collapsing stops as soon as the triangle count reaches the target or when no valid collapse remains, so a heavily constrained mesh may end above the target.

### 4. Quadric error metric

For every face with unit normal `n`, area `A` and any of its points `x0`, define the plane `p = (a, b, c, d)` with `(a, b, c) = n` and `d = -n . x0`. Its fundamental quadric is the 4x4 matrix `K = p p^T`, so that `v^T K v` is the squared distance from `v = (x, y, z, 1)` to the plane. The quadric of a vertex is the **area-weighted** sum over its incident faces:

```
Q(vertex) = sum over incident faces of  A_face x K_face
```

Degenerate faces (practically zero area) are ignored. The cost of collapsing the edge between vertices 1 and 2 into a point `v` is

```
cost(v) = v^T (Q1 + Q2) v
```

### 5. Choosing where the merged vertex goes

For every edge a set of candidate positions is built and the one with the smallest cost is kept:

- **Both endpoints on a boundary** (only possible if boundaries are not locked): the edge itself must be a boundary edge; the candidates are the two endpoints.
- **One endpoint on a boundary:** the only candidate is the position of that endpoint, so the boundary does not move.
- **Interior edge, UV preservation off:** the *optimal* position is added when it exists and lies within one edge length of the midpoint. It solves `A x = -b`, where `A` is the upper-left 3x3 block of `Q1 + Q2` and `b` is the first three entries of its last column. The system is solved with Cramer's rule, and it is rejected if `|det A| < 1e-12`. The two endpoints and the midpoint are always candidates.
- **Interior edge, UV preservation on:** the optimal position is not used (it could leave the edge and make the UV interpolation meaningless). The candidates are the endpoints, the midpoint and the points at 25% and 75% along the edge.

The UV of the merged vertex is the linear interpolation of the endpoint UVs, using the parameter `t` obtained by projecting the chosen position onto the edge (clamped to 0..1).

### 6. Validity checks

A candidate collapse is accepted only if all of these hold:

1. **Topology.** OpenMesh's `is_collapse_ok` passes in at least one direction. It implements the link condition, which prevents the collapse from creating non-manifold geometry or pinching the surface.
2. **Locked vertices.** If *Lock Boundary Edges* is on, neither endpoint is on a boundary and the edge is not a boundary edge. If *Preserve UV Mapping* is on, neither endpoint belongs to a UV seam. A seam edge is an interior edge whose two faces disagree on the UV of either endpoint by more than `sqrt(1e-10)`.
3. **Hard edges.** If *Preserve Hard Edges* is on, the angle between the unit normals of the two faces must not exceed the threshold.
4. **Normal flips.** If *Prevent Normal Flipping* is on, every face touching either endpoint (except the faces that disappear) is simulated with the new position. The collapse is rejected if a new face is degenerate or if `dot(n_old, n_new) < 0.3`, which is about 72.5 degrees.
5. **UV distortion.** If *Preserve UV Mapping* is on, every affected face is checked in UV space: its signed UV area must keep its sign (no mirroring or folding), it must not degenerate, and its *UV density* (UV area divided by 3D area) must change by a factor between 1/3 and 3.

### 7. The collapse loop

```
quadrics   <- one quadric per vertex
version[v] <- 0 for every vertex
queue      <- min-heap ordered by cost
for each edge e: if e passes the checks, push (cost, e, positions, uv, version of both ends)

while triangles > target and queue is not empty:
    c <- pop the cheapest candidate
    skip c if its edge or vertices were deleted
    skip c if version[v1] or version[v2] changed since it was queued   (stale entry)
    choose a collapse direction that is_collapse_ok accepts, else skip
    removed <- 2 faces for an interior edge, 1 for a boundary edge
    Q       <- Q(from) + Q(to)
    ring    <- neighbours of both vertices
    collapse the half-edge
    move the surviving vertex to c.position and set its UV
    Q(to)   <- Q
    triangles -= removed
    for each vertex in ring: version++ and re-evaluate every edge incident to it,
                              pushing the new candidates
```

The heap uses **lazy invalidation**: entries are never removed or edited; stale ones are discarded when they are popped, which is detected with the per-vertex version counters. Time is O(n log n) in practice and the process is sequential.

### 8. Export of the result

OpenMesh garbage collection removes deleted elements and normals are recomputed (OpenMesh vertex normals, that is, the normalized sum of the face normals around each welded vertex). The output vertex array is then rebuilt, re-splitting vertices that have different UVs, so UV seams are restored in the exported mesh. Texture and material information is copied from the original mesh.

### 9. Time limit and cancellation

The optimization runs in a worker thread so the interface stays responsive. An atomic cancellation flag is checked while the initial candidates are built and on every iteration of the collapse loop. After 60 seconds the flag is raised, the partial result is discarded and the original model stays as it was.

### 10. Parallel decimation on the server

The decimation of one mesh is sequential, so extra cores do not help with a single mesh. The server therefore uses two levels of parallelism (`server/speedigon_server.cpp` and `mesh_chunking.cpp`):

- **Small meshes (under 100,000 triangles):** different meshes are processed by different threads.
- **Large meshes:** the mesh is cut into chunks of about 25,000 triangles (`triangles / 25000`, at least 2 and at most 512) by recursive median bisection of the face centroids along the longest axis of their bounding box. Each chunk gets its own vertex list and is decimated in parallel with **boundary locking forced on**, so the vertices shared between chunks do not move. The results are merged by identifying vertices that have exactly the same position and UV, degenerate faces are dropped, and all normals are recomputed.

The price is that the seams between chunks are not simplified (about 3% of the triangles for 25,000-triangle chunks), and real boundaries of the model are locked even when the option is off.

### References

- M. Garland and P. S. Heckbert. *Surface Simplification Using Quadric Error Metrics.* SIGGRAPH 1997.
- H. Hoppe. *Progressive Meshes.* SIGGRAPH 1996.
- T. K. Dey, H. Edelsbrunner, S. Guha, D. V. Nekhayev. *Topology Preserving Edge Contraction.* 1999 (the link condition used by `is_collapse_ok`).

## Known limitations

- *Target Triangle Count* and *Maximum Deformation Error* are not used by the algorithm yet; only *Reduction Percentage* defines the result.
- Meshes are optimized one at a time, never across meshes.
- Locked boundaries and UV seams can prevent the target from being reached.
- Local optimization is limited to 100,000 triangles and 60 seconds.
- Speedigon uses CPU cores, not a GPU. It helps with very large meshes, but for small models the network transfer can make it slower than optimizing locally.
- The purchase is simulated and `/purchase` grants the flag to any authenticated account.
- The command-line tool in `main_cli.cpp` ignores its target-triangles argument; it uses the default reduction percentage.
- macOS only. OpenGL is deprecated on macOS; it still works, but it may disappear in the future.

## Project layout

| File | Purpose |
| --- | --- |
| `main.cpp` | Application, window, user interface, import/export and the optimization job. |
| `optimization_algorithms.h/.cpp` | The decimation algorithm described above. |
| `mesh_data.h` | Mesh data structures. |
| `scene_renderer.h/.cpp` | 3D viewport: sky, baseplate, lighting and model drawing. |
| `theme.h/.cpp` | Default and neon themes. |
| `settings_ui.h/.cpp` | Settings, account and Speedigon windows. |
| `localization.h/.cpp` | Translations (English, Spanish, German, Italian). |
| `app_settings.h/.cpp` | Local settings file. |
| `cloud_client.h/.cpp`, `cloud_config.example.h` | HTTP client for the backend and its configuration. |
| `mesh_codec.h/.cpp` | Binary and base64 encoding of meshes for the optimization server. |
| `mesh_chunking.h/.cpp` | Chunked parallel decimation used by the server. |
| `server/` | Optimization server and its systemd unit. |
| `aws/lambda/` | Lambda function for accounts. |
| `main_cli.cpp` | Minimal command-line tool. |
| `fonts/` | Fonts used by the interface. |
| `stb_image.h` | Image loading. |

## Security notes

- The optimization server in the default setup is reached over plain HTTP, so the session token and the mesh travel unencrypted. Put it behind HTTPS before using real accounts.
- The session token is stored in plain text in `~/.shrinkigon_settings`.
- Never commit `cloud_config.h`, private keys (`*.pem`) or the Lambda environment variables (`TOKEN_SECRET`).
- If you deploy the backend, set an AWS budget alert and restrict the security group; a public optimization server can be abused.

## Third-party software

Dear ImGui (MIT), GLFW (zlib/libpng), FreeType (FreeType License or GPLv2), Assimp (BSD-3-Clause), OpenMesh, libcurl (curl license), cpp-httplib (MIT) and stb_image (public domain or MIT). Check each project's license, including OpenMesh's, for the exact version you redistribute. The fonts in `fonts/` have their own licenses.

## License

Add your license here (for example MIT) and include a `LICENSE` file in the repository.
