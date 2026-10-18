# Find Files

A Windows desktop app for finding files and cleaning up duplicates, built with
MFC. The layout follows the classic Windows 95 **Find: All Files** dialog: search
tabs at the top, Find Now / Stop / New Search buttons on the right, and the
results list below. A **Duplicates** tab adds duplicate-file detection and
cleanup.

## Features

### Search

- **Name & Location**
  - A plain word matches anywhere in the file name, as in the original dialog:
    `doc` finds `Document.txt` and `Report.doc`.
  - Wildcards (`*.doc`) work, and you can give several names separated by `;`
    or `,`.
  - **Look in** accepts one or more folders separated by `;`, environment
    variables such as `%USERPROFILE%`, mapped network drives and UNC paths
    (`\\server\share\folder`). See [Network drives and shares](#network-drives-and-shares).
  - **Include subfolders** searches the whole tree. Junctions and symbolic links
    are skipped, so the search never loops. DFS links and cloud-sync folders
    (such as OneDrive) are followed.
- **Date Modified**: all files, files modified between two dates, or files
  modified in the previous N months or days.
- **Advanced**
  - **Of type**: All files and folders, All files, Documents, Images, Music,
    Videos, Compressed archives, Programs or Source code.
  - **Containing text**: matches text stored as UTF-8/ANSI or UTF-16. It ignores
    case unless **Options → Case Sensitive** is on. Cloud-only and archived
    files are not read (see [Cloud and archived files](#cloud-and-archived-files)).
  - **Size is**: at least or at most N KB.
  - **Include hidden and system files**.
- The search runs on a background thread. Results appear as they are found, and
  **Stop** ends a search at any time.
- **Long paths**: folders deeper than the classic 260-character limit are
  searched too.

### Duplicates

Turn on **Find duplicate files** on the Duplicates tab and choose what makes two
files duplicates:

| Mode | Compares |
| --- | --- |
| Same name and size | File name (ignoring case) and size |
| Same size and content (SHA-256) | Size, then file contents |
| Same name, size and content | All three |

- Content comparison is fast: it hashes the first 64 KB of each candidate and
  reads a whole file only when that hash matches another file's.
- You can ignore files smaller than a given size and skip empty files.
- Results are grouped into sets, with the sets that waste the most space first.
  Each set has a header such as
  `Set 1 — 2 copies of 195 KB each (195 KB reclaimable)`.
- Works on local drives, network shares and mapped drives.

For a step-by-step guide, see [Finding and removing duplicates](#finding-and-removing-duplicates).

### Working with results

- **File**: Open, Open Containing Folder, Delete, Move to Folder, Properties and
  Save Results (CSV, including SHA-256 hashes in duplicate mode).
- **Edit**
  - Copy Full Path, Select All and Invert Selection.
  - **Select Duplicates** selects every copy except one in each set. You choose
    which copy to keep: the first, the newest, the oldest, or the one with the
    shortest path.
- **View**: Large Icons, Small Icons, List, Details, and Arrange Icons by name,
  folder, size, type or date. You can also click a column header to sort.
- **Delete** sends files to the Recycle Bin. Network shares and removable
  drives have no Recycle Bin, so files there are deleted permanently; the
  confirmation says so (**Confirm Permanent Delete**) before anything happens.
  It also warns you if the selection would remove every copy in a duplicate set.
- **Move to Folder** never overwrites: a name clash becomes `name (2).ext`.
- A right-click menu on the results offers the same file commands.
- The **Named** history and the last **Look in** folder are kept between runs,
  under `HKCU\Software\FindFilesMFC`.

### Keyboard shortcuts

| Key | Action |
| --- | --- |
| Enter (in the search fields) | Find Now |
| F5 | Find Now |
| Esc | Stop the search; closes the window when no search is running |
| Enter (in the results) | Open |
| Alt+Enter | Properties |
| Del | Delete (to the Recycle Bin; permanent on network and removable drives) |
| Ctrl+A | Select all |
| Ctrl+C | Copy full path |

## Finding and removing duplicates

### 1. Choose where to look

On the **Name & Location** tab:

- Set **Look in** to the folder or drive to check. To compare across several
  places, separate them with `;`. For example, `D:\Photos; \\nas\backup\Photos`
  finds copies that exist in both places.
- Leave **Include subfolders** checked.
- Leave **Named** empty to compare all files, or narrow it down, e.g. `*.jpg; *.png`.
- Optional: use **Advanced → Of type** (e.g. *Images*) or **Date Modified** to
  limit which files are considered.

### 2. Turn on duplicate checking

On the **Duplicates** tab, check **Find duplicate files** (or use
**Options → Find Duplicate Files**). Then pick a mode:

| Mode | When to use it | Speed |
| --- | --- | --- |
| **Same size and content (SHA-256)** | The safe default. Finds true copies even when they have different names, such as `IMG_0012.jpg` and `IMG_0012 (1).jpg`. | Reads file contents |
| **Same name, size and content** | You only care about copies that also share a name. For example, a backup folder that mirrors the original. | Reads file contents, but fewer candidates |
| **Same name and size** | A quick first look, or very large trees and slow network shares. Files with the same name and size are usually copies, but this isn't guaranteed: check before deleting. | Fast: no file contents are read |

Two more settings:

- **Ignore files smaller than** skips small files. Setting it to around
  100–1,024 KB keeps the results focused on files that actually free space and
  makes the search much faster.
- **Skip empty files** (on by default) leaves out zero-byte files, which would
  otherwise all count as copies of each other.

Click **Find Now** (or press F5). The status bar shows progress: first the
folder scan, then *Comparing file contents… N of M*. Click **Stop** at any
time. A stopped duplicate search shows no results, because sets can't be
confirmed until every candidate has been compared.

### 3. Review the sets

Each set is a collapsible group of identical files. The largest wasted space is
listed first:

```
Set 1  —  3 copies of 1.2 GB each  (2.4 GB reclaimable)
```

The status bar shows the totals, e.g. *12 duplicate file(s) in 5 set(s) — 3.1 GB
can be reclaimed*. Click a header's arrow to collapse a set. Double-click a file
to open it, or use **Open Containing Folder** to see it in Explorer.

### 4. Choose which copies to remove

Use **Edit → Select Duplicates** to select every copy except one in each set.
You choose which copy is kept:

| Option | Keeps |
| --- | --- |
| Keep First in Each Set | The file listed first in the set (sorted by path unless you've re-sorted the list) |
| Keep Newest | The most recently modified copy |
| Keep Oldest | The earliest modified copy, usually the original |
| Keep Shortest Path | The copy nearest the top of the folder tree, e.g. `D:\Photos\a.jpg` rather than `D:\Photos\Old\Backup\a.jpg` |

After that you can still Ctrl+click to change individual files. The status bar
shows how many files are selected and how much space they take.

### 5. Remove them

- **Delete** (Del): sends files on local drives to the Recycle Bin. On network
  shares and removable drives the delete is **permanent**, and the confirmation
  says so. If your selection includes *every* copy in a set, you get an extra
  warning that nothing of that file would be kept.
- **Move to Folder…**: moves the selected copies to a folder you choose, e.g.
  `D:\Duplicates-to-review`, instead of deleting them. Name clashes become
  `name (2).ext`, so nothing is overwritten. This is the safer choice on network
  shares.

Removed or moved files disappear from the list. A set left with only one file
is removed, since that file is no longer a duplicate.

### 6. Keep a record (optional)

**File → Save Results** writes a CSV with the set number, name, folder, size,
type, modified date and SHA-256 hash of every listed file. Save it *before*
deleting if you want a record of what was there.

## Network drives and shares

Duplicate checking works on network locations as well as local drives:

- **Mapped drives** (e.g. `Z:\`) appear in the **Look in** list.
- **UNC paths** (`\\server\share\folder`) can be typed into **Look in** or
  picked with **Browse…**.
- **DFS namespaces** (e.g. `\\corp.local\shares\...`) are followed into their
  target shares.
- **Long paths** on shares are supported.
- Network and local folders can be mixed in one search, e.g.
  `C:\Projects; \\nas\archive\Projects`.

Things to know:

- **Deleting on a share is permanent.** Network shares have no Recycle Bin. The
  confirmation is titled **Confirm Permanent Delete** and says *This cannot be
  undone*. Consider **Move to Folder** instead.
- **Content comparison reads files over the network.** Every candidate file is
  read across the network, so on a large share:
  - use **Ignore files smaller than**;
  - narrow the search with **Named** or **Of type**;
  - or start with **Same name and size**, which reads no file contents.

  The 64 KB pre-hash means a whole file is read only when its start matches
  another file.
- **Mapped drives and "Run as administrator".** Drives mapped in your normal
  session aren't visible to an elevated app. Run Find Files normally, or use
  the UNC path instead.
- **Permissions.** Folders you can't open are skipped silently. Files you can
  see but not read can't be compared by content, so they won't appear in
  content-based sets.

## Cloud and archived files

Some files are listed in a folder but their data isn't stored on the disk:

- OneDrive and other cloud-sync *online-only* files;
- files moved to archive storage (shown as *offline*).

Reading these would download or recall them, so:

- **Same name and size** includes them as usual.
- The **content** modes skip them, so they never appear in content-based sets.
  To include them, make the files available offline first (in OneDrive: right-click
  → **Always keep on this device**).
- **Containing text** doesn't match them.

## Requirements

- Windows 10 or later.
- Visual Studio 2022 or later with the **Desktop development with C++**
  workload, plus the **C++ MFC for latest v143 build tools** component.
- The project uses the v143 toolset, C++17 and the Windows 10 SDK.

## Building

Open `FindFiles.sln` in Visual Studio, choose a configuration (Debug or
Release) and a platform (x64 or x86), and build.

From a Developer Command Prompt:

```bash
msbuild FindFiles.sln /p:Configuration=Release /p:Platform=x64
```

The executable is written to `bin\<Platform>\<Configuration>\FindFiles.exe`.
MFC is linked statically, so the exe runs on another machine without
installing MFC or the Visual C++ runtime.

## Project layout

```
FindFiles.sln
FindFiles/
  FindFiles.cpp        Application class (CWinApp): starts the main dialog
  FindFilesDlg.h/.cpp  Main window: tabs, buttons, results list, menus, file commands
  Pages.h/.cpp         The four tab pages and their validation
  SearchEngine.h/.cpp  Background search, content matching and duplicate hashing
  FindFiles.rc         Dialogs, menus, accelerators and version info
  resource.h           Resource and command IDs
  LongPath.manifest    Opts the exe in to long-path support
  pch.h/.cpp           Precompiled header
```

### How it works

- `CSearchEngine` walks the folders on a worker thread (`AfxBeginThread`).
  - It posts results to the dialog in batches (`WM_APP_RESULTS`), sends
    throttled progress text for the status bar (`WM_APP_STATUS`), and signals
    when it finishes (`WM_APP_DONE`).
  - Stopping sets an atomic flag that the walk and the hashing loops check.
- In duplicate mode the engine collects every match first, then narrows the
  candidates in passes: name and size, then a 64 KB SHA-256 prefix hash
  (Windows CNG / BCrypt), then a full hash.
- The results list uses the shell's system image list for icons. Duplicate sets
  are shown as collapsible list-view groups.
- Folders that are reparse points are only skipped when the reparse tag is a
  *name surrogate* (`IsReparseTagNameSurrogate`): symbolic links, junctions and
  mount points. That prevents loops while still following DFS links and
  cloud-sync folders.
- Files with `FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS` or `FILE_ATTRIBUTE_OFFLINE`
  are never opened for hashing or text search.
- Delete checks each file's location: UNC paths and non-fixed drives
  (`GetDriveType`) get the permanent-delete confirmation.
- Paths longer than `MAX_PATH` are passed to file APIs with the `\\?\` prefix
  (`\\?\UNC\` for shares). The manifest also declares `longPathAware`.

## Notes

- Delete, Open Containing Folder and Properties go through the Windows shell,
  which may not handle paths longer than 260 characters.
- Content comparison reads every candidate file, so a duplicate search over a
  large drive can take a while. **Ignore files smaller than** reduces the work.
