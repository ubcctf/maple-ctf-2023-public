dist: dist/flakey-ci.nix dist/micro.nix dist/flake.nix dist/flake.lock dist/ci.py dist/flag.txt dist/README.md
	zip dist.zip dist/*

dist/flag.txt:
	echo 'no flag here' > dist/flag.txt

dist/%: src/%
	cp $^ dist/
