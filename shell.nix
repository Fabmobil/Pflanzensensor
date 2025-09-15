{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Python environment
    (python311.withPackages (ps: with ps; [
      websockets
      pytest
      pytest-asyncio
      black
      pylint
      setuptools
      pip
      virtualenv
      aiohttp
      aiofiles
      termcolor
      websockets
      rich
      flask
      flask-socketio
      pandoc
    ]))

    # Development tools
    git
    pre-commit
    zsh
    clang-tools_14

    # PDF generation requirements - minimal set
    (texlive.combine {
      inherit (texlive)
        scheme-minimal    # Base LaTeX
        collection-basic
        collection-latex
        collection-latexrecommended
        collection-latexextra
        collection-fontsrecommended
        collection-mathscience  # Includes amsmath and amssymb
        xetex            # For Unicode/font support
        xcolor          # For syntax highlighting
        geometry        # For page margins
        hyperref        # For links
      ;
    })
    pandoc
  ];

  shellHook = ''
    # Create and activate virtual environment if it doesn't exist
    if [ ! -d .venv ]; then
      echo "Creating virtual environment..."
      # Create new venv without pip
      python -m venv .venv
      source .venv/bin/activate
      python -m pip install --upgrade pip
    fi

    # Activate virtual environment
    source .venv/bin/activate

    # Install package in development mode with all dependencies
    echo "Installing package in development mode..."
    PYTHONPATH=$PWD/.venv/lib/python3.11/site-packages:$PYTHONPATH \
    PIP_PREFIX=$PWD/.venv \
    python -m pip install -e .

    # Setup pre-commit hooks if .git directory exists
    if [ -d .git ]; then
      echo "Setting up pre-commit hooks..."
      # First install pre-commit in the virtual environment
      pip install pre-commit
      # Then install the hooks
      pre-commit install
    fi

    echo "Python development environment ready!"
    echo "Virtual environment activated at .venv"
    echo "Run 'deactivate' to exit virtual environment"

    # we use zsh
    zsh
  '';

  # Set environment variables
  PYTHONPATH = "./.venv/lib/python3.11/site-packages:./";
}
