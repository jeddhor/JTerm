# SplitTerm (Qt6 C++ Terminal Emulator)

SplitTerm is a Linux-first, Qt6-based split-pane terminal emulator that uses a real PTY/TTY backend via QTermWidget.

## Features

- Flexible pane splitting:
  - Horizontal and vertical splits
  - Nested split tree supports complex layouts (for example, 12 equal panes or mixed asymmetric grids)
- Tabbed workspaces:
  - Multiple tabs, each with its own split layout
  - Tab IDs and user-editable tab titles
- Pane metadata:
  - Freeform user-editable pane titles
  - Unique pane IDs
- Pane controls:
  - Right-click menu for copy/paste/select-all
  - Right-click rename, split, and close terminal actions
  - Middle-click paste (primary selection)
  - Closing panes auto-reflows remaining panes
- Layout save/load:
  - Save pane tree + splitter geometry to `.json`
  - Load `.json` layouts
- Built-in settings dialog:
  - Default shell selection
  - Maximum pane count
- Appearance:
  - Uses the OS desktop theme and native Qt appearance
  - Terminal rendering uses a dark xterm-like color profile
- Standard edit operations:
  - Copy / Paste / Select All for active pane
- Command-line remote command bridge:
  - Send a command to a running pane by `--pane-id` or `--pane-title`

## Build Requirements

- `g++` with C++20 support
- `make`
- `pkg-config`
- Qt6 development packages:
  - Core
  - Gui
  - Widgets
  - Network
- QTermWidget development package (`qtermwidget6`)
- Qt Meta-Object Compiler (`moc` or `moc-qt6`)

## Ubuntu Packages

```bash
sudo apt update
sudo apt install -y build-essential make pkg-config qt6-base-dev qt6-base-dev-tools libqtermwidget6-2-dev
```

On some Ubuntu releases, the package may be named `libqtermwidget6-dev` instead.

If launch or link errors mention `utf8proc`, install:

```bash
sudo apt install -y libutf8proc3
```

For better desktop integration on Ubuntu/KDE environments, these are recommended:

```bash
sudo apt install -y qdbus-qt6 qt6-gtk-platformtheme qt6-wayland
```

## Build

```bash
make
```

## Run

```bash
./splitterm
```

On Windows with MinGW, executable is `splitterm.exe`.

## Remote CLI Usage

Send a command to a running SplitTerm instance:

```bash
./splitterm --send "ls -la" --pane-id 3
```

Or by pane title:

```bash
./splitterm --send "pwd" --pane-title "Logs"
```

If no running instance is listening, the CLI command returns an error.

## Layout JSON Shape

Saved layout JSON contains:

- `version`
- `tabs` array:
  - each tab has `id`, `title`, and `root`
- each `root` node:
  - `type: pane` with `id` + `title`
  - `type: splitter` with `orientation`, `sizes`, and `children`

This preserves nested split structure and pane metadata.
