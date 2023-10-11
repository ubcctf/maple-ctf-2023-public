CFLAGS?=-Isrc -Wall -Wextra -Os -g
LDFLAGS?=-T src/link.x

TARGET=build/lost-in-space

all: build $(TARGET)

build:
	mkdir -p build

$(TARGET): build/lost-in-space.o
	$(CC) -o $@ $(LDFLAGS) $<
	cp $(TARGET) $(TARGET).dbg
	strip $@
	sstrip $@

build/lost-in-space.o: src/lost-in-space.c src/syscall.h
	$(CC) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf build
