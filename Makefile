APP := splitterm

ifeq ($(OS),Windows_NT)
APP := splitterm.exe
MKDIR_P := if not exist build mkdir build
RM_RF := if exist build rmdir /S /Q build & if exist $(APP) del /Q $(APP)
else
MKDIR_P := mkdir -p build
RM_RF := rm -rf build $(APP)
endif

CXX ?= g++
MOC ?= $(shell command -v moc-qt6 2>/dev/null || command -v moc 2>/dev/null)
PKG_CONFIG ?= pkg-config

QT_PACKAGES := Qt6Core Qt6Gui Qt6Widgets Qt6Network
CXXFLAGS := -std=c++20 -O2 -Wall -Wextra -pedantic -Iinclude $(shell $(PKG_CONFIG) --cflags $(QT_PACKAGES))
LDFLAGS := $(shell $(PKG_CONFIG) --libs $(QT_PACKAGES))

SRC := $(wildcard src/*.cpp)
MOC_HEADERS := include/TerminalView.h include/TerminalPane.h include/SettingsDialog.h include/CommandServer.h include/MainWindow.h
MOC_SRC := $(patsubst include/%.h,build/moc_%.cpp,$(MOC_HEADERS))
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC)) $(patsubst build/%.cpp,build/%.o,$(MOC_SRC))

.PHONY: all clean run

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

clean:
	$(RM_RF)
