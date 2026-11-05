{
  description = "Entwicklungsumgebung fuer den Fabmobil-Pflanzensensor (PlatformIO, ESP8266/ESP32)";

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
            # Fuer den Blick auf die CI-Laeufe nach einem Push
            # (gh run list / gh run view) und die uebrige GitHub-Arbeit.
            gh
            pre-commit
            clang-tools
            pandoc
            cppcheck
            # Achtung: liefert python3.12, aber NICHT das blanke python3 -
            # das kommt aus den Abhaengigkeiten von pre-commit/platformio und
            # ist derzeit 3.14. Die Projektskripte (firmware/tools/*.py,
            # webflasher/build_site.py) kommen mit der Standardbibliothek aus.
            python312
            platformio
            # Native Tests (pio test -e native, pre-commit) binden <openssl/evp.h>
            # ein und linken -lcrypto (AES-GCM/SHA als Referenz); die CI nimmt
            # dafür libssl-dev.
            openssl
            # Für firmware/tools/run_js_tests.sh - node bringt Testläufer und
            # Zusicherungen selbst mit, es gibt keine npm-Abhängigkeiten.
            # (npm braucht nur der Webflasher-Bau für esp-web-tools.)
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
