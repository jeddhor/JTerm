# SplitTerm (Qt6 C++ Terminal Emulator)

SplitTerm is a Linux-first, Qt6-based split-pane terminal emulator that uses a real PTY/TTY backend via QTermWidget.

## Features

- Flexible pane splitting:
  - Horizontal and vertical splits
  - Nested split tree supports complex layouts (for example, 12 equal panes or mixed asymmetric grids)
- Pane metadata:
  - Freeform user-editable pane titles
  - Unique pane IDs
- Layout save/load:
  - Save pane tree + splitter geometry to `.json`
  - Load `.json` layouts
- Built-in settings dialog:
  - Theme selection
  - Default shell selection
  - Maximum pane count
- Multiple themes:
  - Breeze Light
  - Breeze Dark
  - Solarized Light
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
- `root` node:
  - `type: pane` with `id` + `title`
  - `type: splitter` with `orientation`, `sizes`, and `children`

This preserves nested split structure and pane metadata.
