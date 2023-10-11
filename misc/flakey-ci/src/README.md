# flakey-ci

## run (vm; easier)

```
nix run .#nixosConfigurations.boxMicro.config.microvm.declaredRunner
nc localhost 1337
```

## run (docker)

```
nix run .#dockerImage.copyToDockerDaemon
docker run --rm --device=/dev/kvm -p 1337:1337 flakey-ci
nc localhost 1337
```
