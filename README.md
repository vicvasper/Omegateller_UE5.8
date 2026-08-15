# OmegaTeller

<img width="1902" height="1019" alt="image" src="https://github.com/user-attachments/assets/61240ffa-1a2b-4aa2-a97c-42b7b5aebed2" />


Narrative design plugin for Unreal Engine 5.8. It parses a Game Design Document, extracts the characters that appear in it, and builds a branching story tree per character as a node graph you can edit by hand: merge two beats into one, split a timeline into a new branch, insert an event, delete a node, and the surrounding nodes get rewritten so the story still reads as one coherent thread.

The editor runs inside Unreal as an embedded web view (HTML5 canvas, drag and drop, zoom and pan), not a Slate graph. All the narrative logic (parsing, merging, coherence pass) lives in C++ on the Unreal side; the browser only renders the graph and sends user actions back.

**Status: experimental.** The plugin is marked as experimental in `OmegaTellerPlugin.uplugin` (Unreal will warn you when enabling it). Core parsing, merging, and export work, but expect rough edges, especially around AI coherence quality with smaller local models. Issues and pull requests are welcome.

## AI backend

OmegaTeller ships with its own local language model, so it works out of the box without any account, API key, or network access. On first launch it starts a small bundled inference server (llama.cpp) with a lightweight instruction-tuned model, and every parse, merge, split, and repopulate goes through it.

You are not locked into that model:

- Point it at any other OpenAI-compatible server (Ollama, LM Studio, a self-hosted endpoint) by setting `LocalEndpoint` in `Config/OmegaTellerConfig.ini` and turning off `bAutoStartLocalServer`.
- Swap the bundled model for a stronger one by dropping a different `.gguf` file into `ThirdParty/Models/`.
- Add a Gemini or Groq API key in the same ini file as a remote fallback if the local model is unavailable.

If nothing responds, the plugin falls back to a plain heuristic extractor so the tool degrades instead of breaking.

### Getting the local model

To keep the repository small, the model file itself is not included. Download one GGUF-format instruction model (Qwen2.5-1.5B-Instruct Q4_K_M is a good default, about 1 GB) and place it in:

```
Plugins/OmegaTellerPlugin/ThirdParty/Models/
```

Any single `.gguf` file in that folder is picked up automatically. Without one, OmegaTeller still runs; it just uses the remote or heuristic path instead of the local server.

## Features

- GDD parsing: paste a design document and extract characters, roles, and story beats without any fixed template.
- Per-character narrative trees, linear or branching, with Start, Event, Decision, Branch, and End node types.
- Node graph editing: drag, connect, merge across characters, split a shared scene back into separate timelines.
- Coherence pass: after any structural edit, the nodes around the change are regenerated so the timeline still flows as one story instead of leaving stale text behind.
- Language preserved end to end: generated content stays in whatever language the source text was written in.
- JSON export/import for trees and characters, plus a plain text story export.
- Works fully offline with the bundled model, or connects to your own backend.

## Installation

1. Copy `OmegaTellerPlugin` into your project's `Plugins/` folder.
2. Regenerate project files and build the project (Unreal Engine 5.8, Visual Studio 2022).
3. Enable the plugin from Edit > Plugins > OmegaTellerPlugin and restart the editor.
4. Optional: add a model file as described above for local AI.

## Usage

Open the editor from Window > OmegaTeller, or from the button on the Play toolbar.

1. Click Parse GDD and paste your design document text.
2. The characters found in the text appear as separate timelines, each with its own node graph.
3. Edit nodes directly: drag to reposition, drag one node onto another to merge them, use the split action to break a shared scene back apart.
4. Export the finished trees to JSON for use at runtime, or export a character's timeline as plain text.

## Configuration

`Config/OmegaTellerConfig.ini` controls the AI backend and a few editor defaults:

| Key | Purpose |
|---|---|
| `bAutoStartLocalServer` | Start the bundled local model server automatically |
| `LocalPort` | Port for the bundled server |
| `LocalEndpoint` | Override to point at any other OpenAI-compatible endpoint |
| `LocalModel` | Model name sent in requests |
| `ModelFile` | Pick a specific `.gguf` file if you keep more than one in `ThirdParty/Models` |
| `GeminiKey` / `GroqKey` | Optional remote fallback if you prefer a cloud model |

## Architecture

Runtime module (`OmegaTellerPlugin`):

- `UOmegaTellerManager` - singleton entry point, owns trees and characters, resolves which AI backend to use.
- `UOmegaNode` / `UOmegaTree` / `UOmegaCharacter` - narrative data model, connection and traversal logic, JSON serialization.
- `URorkToolkitAdapter` - GDD text analysis: character candidate extraction, story beat splitting, attribute inference.

Editor module (`OmegaTellerPluginEditor`):

- `SOmegaModernEditor` - hosts the embedded web view and the bridge between C++ and the JS graph editor.
- `FOmegaLocalAI` - manages the bundled inference server and builds the AI configuration handed to the browser.
- `Content/Web/OmegaGraphEditor.html` - the node graph UI and coherence logic that runs in the browser.

## License

Plugin code is released under the MIT license (see `LICENSE`). The bundled `llama.cpp` binaries under `ThirdParty/LlamaServer` are distributed under their own MIT license from the [llama.cpp project](https://github.com/ggml-org/llama.cpp).
