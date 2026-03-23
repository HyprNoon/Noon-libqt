# Downloader Components

## Overview

The downloader is split into two C++ classes and one QML delegate, backed by a JSON file for persistence. `DownloadItem` owns a single KIO transfer job. `DownloadModel` owns a list of items and exposes them to QML via roles. The QML delegate renders one item from that list.

---

## DownloadItem

`dw_item.hpp` / `dw_item.cpp`

Represents a single file transfer. Wraps a `KIO::FileCopyJob` and tracks all transfer metadata.

### State Machine

```
Queued → Running → Finished
                 → Canceled
       → Paused  → Running
```

| State    | Job exists | Persisted | Restored with job |
|----------|------------|-----------|-------------------|
| Queued   | No         | Yes       | No                |
| Running  | Yes        | Yes       | Yes               |
| Paused   | Yes (suspended) | Yes  | Yes (then suspended) |
| Canceled | No         | Yes       | No                |
| Finished | No         | Yes       | No                |

### Properties

| Property        | Type    | Description                                      |
|-----------------|---------|--------------------------------------------------|
| `label`         | QString | Display name, constant after construction        |
| `url`           | QUrl    | Source URL, constant after construction          |
| `destination`   | QUrl    | Target file path, constant after construction    |
| `progress`      | int     | Percent complete (0–100)                         |
| `state`         | State   | Current state enum value                         |
| `totalBytes`    | qint64  | Total file size in bytes, -1 if unknown          |
| `receivedBytes` | qint64  | Bytes transferred so far                         |
| `speed`         | qint64  | Current transfer speed in bytes/second           |
| `eta`           | int     | Estimated seconds remaining, -1 if unknown       |

### Construction

There are two entry points:

- `DownloadItem(label, url, destination, parent)` — fresh download, starts immediately in `Queued` transitioning to `Running`.
- `DownloadItem::fromJson(obj, parent)` — restores from a JSON object. Inert for `Canceled` and `Finished`. Restores `Paused` by starting the job then immediately suspending it.

### Serialization

`toJson()` writes all properties including `totalBytes` and `receivedBytes` so restored items can display meaningful size information before KIO reports live data.

### ETA Calculation

`recalcEta()` is called whenever `speed`, `totalBytes`, or `receivedBytes` changes:

```
eta = (totalBytes - receivedBytes) / speed
```

Returns -1 when speed is zero or total size is unknown. Speed and ETA are also zeroed immediately on suspend so the UI never shows stale values while paused.

---

## DownloadModel

`dw_model.hpp` / `dw_model.cpp`

A `QAbstractListModel` that owns a list of `DownloadItem` pointers and exposes them to QML. Also handles persistence to a JSON file.

### Roles

| Role              | Type           | Description                        |
|-------------------|----------------|------------------------------------|
| `label`           | QString        | Item display name                  |
| `url`             | QUrl           | Source URL                         |
| `destination`     | QUrl           | Target file path                   |
| `progress`        | int            | Percent complete                   |
| `state`           | DownloadItem::State | Current state                 |
| `item`            | DownloadItem*  | Direct pointer to the item object  |
| `totalBytes`      | qint64         | Total file size in bytes           |
| `receivedBytes`   | qint64         | Bytes received so far              |
| `speed`           | qint64         | Bytes per second                   |
| `eta`             | int            | Estimated seconds remaining        |

### QML API

| Method                              | Description                                         |
|-------------------------------------|-----------------------------------------------------|
| `add(url, destination, label)`      | Creates and starts a new download                   |
| `remove(index)`                     | Cancels and permanently removes an item             |
| `pause(index)`                      | Suspends the KIO job for an active download         |
| `resume(index)`                     | Resumes a suspended KIO job                         |
| `cancel(index)`                     | Cancels the transfer, item remains in list as Canceled |
| `indexOfUrl(url)`                   | Returns the row of the item with matching URL or -1 |
| `isDownloading(url)`                | Returns true if a matching URL is in the list       |

### Persistence

Controlled by the `jsonPath` property. The model loads on `jsonPath` assignment and saves on every state change. Progress saves are throttled — only written when progress is a multiple of 10 to avoid hammering the disk on every percent tick.

Items are skipped on load if their state is `Canceled`. `Finished` items are fully restored and displayed inertly.

### Signal Routing

Two separate private slots handle item signals to avoid triggering disk writes on high-frequency byte updates:

- `onItemDataChanged` — handles `progressChanged` and `stateChanged`. Emits `dataChanged` for those roles and conditionally calls `save()`.
- `onItemBytesChanged` — handles `totalBytesChanged`, `receivedBytesChanged`, `speedChanged`, `etaChanged`. Emits `dataChanged` for those roles only, never calls `save()`.

---

## QML Delegate

The delegate renders a single row from `DownloadModel`, accessed via `DownloadService.model`.

### Collapsed State

Shows a circular progress indicator, the item label, file extension, and an expand toggle button. Canceled and finished items show a strikethrough on the label and extension.

### Expanded State

Shows a details grid with the following fields:

| Field       | Source                              | Format              |
|-------------|-------------------------------------|---------------------|
| Downloaded  | `receivedBytes`                     | Human-readable size |
| Total       | `totalBytes`                        | Human-readable size |
| Remaining   | `totalBytes - receivedBytes`        | Human-readable size |
| Speed       | `speed`                             | Human-readable size + /s |
| ETA         | `eta`                               | h/m/s breakdown     |
| State       | `state`                             | String label        |

### Action Buttons

| Condition       | Buttons shown                                              |
|-----------------|------------------------------------------------------------|
| Running         | Pause / Cancel                                             |
| Paused          | Resume / Cancel                                           |
| Canceled        | Redownload (removes old entry then calls `add`)           |
| Finished        | Dismiss (calls `remove`)                                  |

### Formatting Helpers

All formatting is handled by functions on the root item so they are accessible from the inline `InfoSection` component:

- `formatBytes(bytes)` — renders `-1` and `0` as `—`, otherwise scales to B / KB / MB / GB.
- `formatEta(seconds)` — renders `-1` as `—`, otherwise formats as `Xh Xm Xs`.
- `formatState(state)` — maps the enum to a display string.
