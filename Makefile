APP := jterm
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

ifeq ($(OS),Windows_NT)
APP := jterm.exe
MKDIR_P := if not exist build mkdir build
RM_RF := if exist build rmdir /S /Q build & if exist $(APP) del /Q $(APP)
else
MKDIR_P := mkdir -p build
RM_RF := rm -rf build $(APP)
endif

CXX ?= g++
MOC ?= $(firstword \
	$(wildcard /usr/lib/qt6/libexec/moc) \
	$(shell command -v moc-qt6 2>/dev/null) \
	$(shell command -v moc 2>/dev/null))
PKG_CONFIG ?= pkg-config

QTERM_CANDIDATE_PKGS := qtermwidget6 qtermwidget6-qt6
QTERM_DISCOVERED_PKGS := $(shell $(PKG_CONFIG) --list-all 2>/dev/null | awk '{print $$1}' | grep -Ei '^qtermwidget([0-9]+)?(-qt[0-9]+)?$$')
QTERM_CANDIDATE_PKGS += qtermwidget qtermwidget-qt6 qtermwidget5 qtermwidget5-qt5 $(QTERM_DISCOVERED_PKGS)
QTERM_PKG ?= $(firstword $(foreach p,$(QTERM_CANDIDATE_PKGS),$(if $(shell $(PKG_CONFIG) --exists $(p) 2>/dev/null && echo yes),$(p))))
QTERM_FOUND := $(shell $(PKG_CONFIG) --exists $(QTERM_PKG) 2>/dev/null && echo yes)

ifeq ($(strip $(MOC)),)
$(error Qt6 moc not found. Install qt6-base-dev-tools or run make with MOC=/full/path/to/moc)
endif

ifeq ($(strip $(QTERM_FOUND)),)
$(error qtermwidget pkg-config entry not found. Install qtermwidget dev package and pkg-config, then run 'pkg-config --list-all | grep -i qtermwidget' and build with make QTERM_PKG=<name>)
endif

QT_PACKAGES := Qt6Core Qt6Gui Qt6Widgets Qt6Network
QTERM_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(QTERM_PKG) 2>/dev/null)
QTERM_LIBS := $(shell $(PKG_CONFIG) --libs $(QTERM_PKG) 2>/dev/null)

ifeq ($(strip $(QTERM_LIBS)),)
QTERM_LIBS := -lqtermwidget6
endif

CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic -Iinclude $(shell $(PKG_CONFIG) --cflags $(QT_PACKAGES)) $(QTERM_CFLAGS)
LDFLAGS := $(shell $(PKG_CONFIG) --libs $(QT_PACKAGES)) $(QTERM_LIBS)

SRC := $(wildcard src/*.cpp)
MOC_HEADERS := include/SnapSplitter.h include/LayoutEditorDialog.h include/StartupScriptDialog.h include/TerminalView.h include/TerminalPane.h include/SettingsDialog.h include/LlmChatDialog.h include/CommandServer.h include/MainWindow.h
MOC_SRC := $(patsubst include/%.h,build/moc_%.cpp,$(MOC_HEADERS))
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC)) $(patsubst build/%.cpp,build/%.o,$(MOC_SRC))

.PHONY: all clean run install uninstall

all: $(APP)

$(APP): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/moc_%.cpp: include/%.h | build
	$(MOC) $< -o $@

build/moc_%.o: build/moc_%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build:
	$(MKDIR_P)

run: $(APP)
	./$(APP)

install: $(APP)
ifeq ($(OS),Windows_NT)
	@echo "install target is intended for Linux/macOS."
else
	install -d "$(BINDIR)"
	install -m 0755 "$(APP)" "$(BINDIR)/jterm"
	@echo "Installed $(APP) to $(BINDIR)/jterm"
endif

uninstall:
ifeq ($(OS),Windows_NT)
	@echo "uninstall target is intended for Linux/macOS."
else
	rm -f "$(BINDIR)/jterm"
	@echo "Removed $(BINDIR)/jterm"
endif

clean:
	$(RM_RF)
