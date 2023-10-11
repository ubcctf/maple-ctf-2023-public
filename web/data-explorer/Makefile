dist.zip: dist/src/server.py dist/src/run.sh dist/src/templates/*.html dist/Dockerfile
	zip -r $@ dist

dist/Dockerfile: Dockerfile
	cp $^ $@

dist/src/%: src/%
	mkdir -p $(dir $@)
	cp $^ $(dir $@)
