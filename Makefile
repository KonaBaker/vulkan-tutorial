CFLAGS = -std=c++17 -O2
LDFLAGS = $(shell pkg-config --libs glfw3 vulkan)

vulkan-tutorial: main.cpp
	g++ $(CFLAGS) -o vulkan-tutorial main.cpp $(LDFLAGS)

.PHONY: test clean

test: vulkan-tutorial
	./vulkan-tutorial

clean:
	rm -f vulkan-tutorial
