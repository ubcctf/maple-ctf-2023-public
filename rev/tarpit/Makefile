dist.zip: dist/src/chal dist/Dockerfile
	rm dist/src/chal.c
	zip -r $@ dist

dist/Dockerfile: Dockerfile
	cp $^ $@

dist/src/chal: dist/src/chal.c
	gcc -s -o dist/src/chal src/chal.c

dist/src/%: src/%
	mkdir -p $(dir $@)
	cp $^ $(dir $@)