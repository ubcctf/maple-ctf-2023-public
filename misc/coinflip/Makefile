dist.zip: dist/src/server.py dist/Dockerfile
	zip -r $@ dist

dist/Dockerfile: Dockerfile
	cp $^ $@

dist/src/%: src/%
	mkdir -p $(dir $@)
	cp $^ $(dir $@)
