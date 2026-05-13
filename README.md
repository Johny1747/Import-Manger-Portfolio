# Runtime Asset Importer & Serialization Manager (Unreal Engine C++)

## Overview
This snippet demonstrates a production-grade C++ subsystem designed for dynamic, runtime asset management in Unreal Engine. It bypasses the standard Editor asset pipeline to allow users to ingest external files (like floor plans or logos) during runtime, securely serializing them for cloud storage, project sharing, and session persistence.

## ⚙️ Core Technical Highlights

This code highlights deep engine integration, specifically focusing on safe memory manipulation, data serialization, and subsystem architecture.

* **Direct Memory Management (`IImageWrapper`):** Bypasses standard texture loading by reading raw byte arrays from disk, auto-detecting image formats, and decompressing them. Safely allocates transient `UTexture2D` objects in RAM, explicitly locking and unlocking texture platform data (`BulkData.Lock(LOCK_READ_WRITE)`) to write uncompressed BGRA pixels directly to the GPU.
* **Robust JSON Serialization:** Converts raw binary data into Base64 strings to survive JSON serialization. Implements custom parsing logic to encode/decode structured metadata (Image Name, MD5 Hash Codes, Raw Data) using Unreal's `FJsonObject` and `TJsonReader`.
* **Data Integrity & Hashing:** Generates unique, timestamp-salted MD5 hashes for every imported file to guarantee unique identification and prevent data collisions within the local register.
* **Asynchronous-Ready Architecture:** Designed as a decoupled manager that queries the `GameInstance` for related subsystems (e.g., `UFileLibraryManager`), allowing for scalable read/write operations without tightly coupling to gameplay classes.

## 📂 Key Responsibilities

* **`LoadTextureFromRawData()`**: The core conversion engine. Translates raw `TArray<uint8>` into GPU-ready textures via memory locks.
* **`ImgMetaToJson()` / `JsonToImgMeta()`**: The serialization bridge, handling the heavy lifting of encoding binary data into text-safe formats.
* **Register Management**: Maintains an in-memory `TArray` register of all imported assets, ensuring O(N) duplicate checks and seamless syncing to a local `Register.json` configuration file.

*Note: This is an isolated snippet from a larger, proprietary data-driven application architecture.*
