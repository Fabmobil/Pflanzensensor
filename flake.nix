{
  description = "Entwicklungsumgebung fuer den Fabmobil-Pflanzensensor (PlatformIO/ESP8266)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            git
            pre-commit
            clang-tools
            pandoc
            cppcheck
            python312
            platformio
          ];

          shellHook = ''
            if [ -d .git ]; then
              pre-commit install --allow-missing-config >/dev/null
            fi
          '';
        };
      });
}
