CC := gcc
CCFLAGS := -I . -fpie -pie -fstack-protector-all
CCFLAGS += -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now
BFLAGS := --no-lines

ifeq ($(DEBUG), 1)
	CCFLAGS += -g -Wall -Wextra -Wpedantic
# BFLAGS += -Wcounterexamples
else
	CCFLAGS += -O1
endif

OUTDIR := out
HDIR := $(OUTDIR)/handout

all: $(OUTDIR)/tetrominobot |$(OUTDIR)

clean:
	rm -rf $(OUTDIR)

run: $(OUTDIR)/tetrominobot
	tr -d '\n' < simple.tbot | ./$(OUTDIR)/tetrominobot -n -d

.PHONY: all clean run tx_clean

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/parse.c: parse.y |$(OUTDIR)
	bison -d $< $(BFLAGS) -o $@

$(OUTDIR)/parse: $(OUTDIR)/parse.c |$(OUTDIR)
	$(CC) -c $(CCFLAGS) $< -o $@

$(OUTDIR)/robot: robot.c $(OUTDIR)/parse |$(OUTDIR)
	$(CC) -c $(CCFLAGS) $< -o $@

$(OUTDIR)/game: game.c |$(OUTDIR)
	$(CC) -c $(CCFLAGS) $< -o $@

$(OUTDIR)/tetrominobot: tetrominobot.c $(OUTDIR)/game $(OUTDIR)/robot $(OUTDIR)/parse |$(OUTDIR)
	$(CC) $(CCFLAGS) $^ -o $@


D_IMG := tbot_img
D_CON := tbot_container
D_BUILD := /build

$(HDIR):
	mkdir -p $(HDIR)

handouts: robot.c game.c tetrominobot.c player-manual.org makefile $(OUTDIR)/docker_tbot_cont |$(HDIR)
	docker restart $(D_CON)
	docker cp . $(D_CON):$(D_BUILD)
	docker exec $(D_CON) /bin/bash -c 'cd $(D_BUILD) && make'
ifneq ($(DEBUG),1)
	docker exec $(D_CON) /bin/bash -c 'strip --strip-all $(D_BUILD)/$(OUTDIR)/tetrominobot'
endif
	docker cp $(D_CON):$(D_BUILD)/$(OUTDIR)/tetrominobot $(HDIR)
	docker cp $(D_CON):/lib/x86_64-linux-gnu/libc.so.6 $(HDIR)
	docker cp $(D_CON):/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 $(HDIR)
	docker stop $(D_CON)
	cp ./player-manual.org $(HDIR)
	cp ./simple.tbot $(HDIR)

$(OUTDIR)/docker_tbot_image: dockerfile |$(OUTDIR)
	touch $@
# clean
	docker stop $(D_CON) || true
	docker container rm --volumes $(D_CON) || true
	docker image rm $(D_IMG) || true
# build
	docker build . -t $(D_IMG) --platform=linux/amd64

$(OUTDIR)/docker_tbot_cont: $(OUTDIR)/docker_tbot_image |$(OUTDIR)
	touch $@
# clean
	docker stop $(D_CON) || true
	docker container rm --volumes $(D_CON) || true
# build
	docker run --platform=linux/amd64 -dit --name $(D_CON) $(D_IMG)

clean_docker:
	rm $(OUTDIR)/docker*
	docker stop $(D_CON) || true
	docker container rm --volumes $(D_CON) || true
	docker image rm $(D_IMG) || true
