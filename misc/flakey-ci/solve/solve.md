# flakey-ci solution

To solve flakey-ci, you just need to create a nix flake that invokes shellcode
as a post-build-hook. The notable thing here is that the shellcode is created by
the flake itself!

The way I created this flake was by doing:

`nix build --print-out-path .#ohno`

then putting that value plus `/bin/nyanya` into `nixConfig.post-build-hook`.
