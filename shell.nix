{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Development tools
    git
    pre-commit
    zsh
    clang-tools_14
    pandoc
    cppcheck
  ];

  shellHook = ''
    # Setup pre-commit hooks if .git directory exists
    if [ -d .git ]; then
      echo "Setting up pre-commit hooks..."
      # First install pre-commit in the virtual environment
      pip install pre-commit
      # Then install the hooks
      pre-commit install
    fi
    # we use zsh
    zsh
  '';
}
