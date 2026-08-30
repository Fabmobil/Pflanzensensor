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
            # Achtung: liefert python3.12, aber NICHT das blanke python3 -
            # das kommt aus den Abhaengigkeiten von pre-commit/platformio und
            # ist derzeit 3.14. Die Projektskripte (bump_version.py,
            # compress_assets.py, generate_md5.py) kommen mit der
            # Standardbibliothek aus, die Nebenversion spielt also keine Rolle.
            python312
            platformio
            # Für ./run_js_tests.sh - node bringt Testläufer und
            # Zusicherungen selbst mit, es gibt keine npm-Abhängigkeiten.
            nodejs_22
          ];

          shellHook = ''
            if [ -d .git ]; then
              pre-commit install --allow-missing-config >/dev/null
            fi
          '';
        };
      });
}
