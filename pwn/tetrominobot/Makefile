all: tetrominobot.zip

dist:
	mkdir -p dist
	make -C src handouts
	cp -r src/out/handout/* dist/

tetrominobot.zip: dist
	cd dist && zip tetrominobot.zip \
		ld-linux-x86-64.so.2 \
		libc.so.6 \
		player-manual.org \
		simple.tbot \
		tetrominobot

clean:
	make -C src clean

.PHONY: clean dist tetrominobot.zip tx

# makefile crimes for my own debugging
# make tx to make handouts on the x86 vm, which spins up docker and runs make
# theoretically might be ok to to just make handouts on docker locally

VM_IP := vm

tx:
	rsync -rva --exclude src/out --exclude dist --exclude .git . $(VM_IP):tbot
	ssh -A $(VM_IP) "cd tbot && make clean && make"
	scp -r $(VM_IP):~/tbot/dist ./
