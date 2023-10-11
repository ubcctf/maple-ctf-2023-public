{
  outputs = { self, nixpkgs }: {
    packages.x86_64-linux.db = let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in with pkgs;
      stdenv.mkDerivation {
        name = "db";
        src = ./.;
        nativeBuildInputs = [ flex bison patchelf ];
        makeFlags = [ "CROSS=" "R=1" ];
        installPhase = ''
          mkdir $out
          cp build/db build/db.dbg $out
        '';
      };
    defaultPackage.x86_64-linux = self.packages.x86_64-linux.db;
  };
}
