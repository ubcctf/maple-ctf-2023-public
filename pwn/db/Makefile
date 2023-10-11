CROSS=$(if $(R),x86_64-unknown-linux-gnu-,)
CC=$(CROSS)cc
STRIP=$(CROSS)strip
SIZE=$(CROSS)size
REL_CFLAGS=\
	-DNDEBUG \
	-Os -flto -frandom-seed=0 \
	--param=max-inline-functions-called-once-insns=100 \
    -fPIE -pie -fstack-protector-all -D_FORTIFY_SOURCE=2 \
	-Wl,-z,relro,-z,now \
	-ffunction-sections -fdata-sections -Wl,--gc-sections
DBG_CFLAGS=-Og -fsanitize=undefined,leak,address
DYNAMIC_LINKER=/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
CFLAGS=-gdwarf-4 -Wall -Wextra -Wno-unused-parameter -Isrc $(if $(R),$(REL_CFLAGS),$(DBG_CFLAGS))

all: build build/db

build/db: src/db.c build/lex.yy.c build/parser.tab.c src/db.h
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) src/db.c build/lex.yy.c build/parser.tab.c $(LDLIBS)
ifdef R
	patchelf --set-interpreter $(DYNAMIC_LINKER) --remove-rpath $@
	cp $@ build/db.dbg
	$(STRIP) $@
	$(SIZE) $@
endif

build/parser.tab.c build/parser.tab.h: src/parser.y src/db.h
	bison -o build/parser.tab.c -d $<

build/lex.yy.c: src/scan.l src/db.h build/parser.tab.h
	flex --always-interactive -o $@ $<

build:
	mkdir -p build

.PHONY: clean
clean:
	rm -rf build
