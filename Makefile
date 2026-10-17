.PHONY: build

EXE = vulkan_basics
CONFIG = Debug

all: compile run

build:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=$(CONFIG)

compile:
	cmake --build build

run:
	./$(EXE)

clean:
	rm -rf build $(EXE)
