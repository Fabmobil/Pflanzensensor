import re
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple


def format_error(message):
    return f"ERROR: {message}"


def format_warning(message):
    return f"WARNING: {message}"


class BuildError(NamedTuple):
    file: str
    line: int
    col: int
    type: str
    message: str
    symbol: str = ""
    first_defined: str = ""


class ErrorParser:
    def __init__(self):
        self.errors: List[BuildError] = []
        self.multiple_defs: Dict[str, List[str]] = {}

        # Compile regex patterns
        self.linker_pattern = re.compile(
            r"(?P<file>.+?):(?P<line>\d+):(?P<col>\d+):\s*(?P<type>warning|error):\s*(?P<message>.+)"
        )
        self.multiple_def_pattern = re.compile(
            r'(?P<file>.+?):\(.+?\):\s*multiple definition of\s*[\'"](?P<symbol>.+?)[\'"];\s*(?P<first_def>.+?):\s*first defined here'
        )

    def parse_line(self, line: str) -> None:
        # Check for multiple definition errors
        multiple_def_match = self.multiple_def_pattern.match(line)
        if multiple_def_match:
            groups = multiple_def_match.groupdict()
            symbol = groups["symbol"]
            if symbol not in self.multiple_defs:
                self.multiple_defs[symbol] = []
            self.multiple_defs[symbol].append(groups["file"])
            self.errors.append(
                BuildError(
                    file=groups["file"],
                    line=0,
                    col=0,
                    type="error",
                    message=f"Multiple definition of '{symbol}', first defined in {groups['first_def']}",
                    symbol=symbol,
                    first_defined=groups["first_def"],
                )
            )
            return

        # Check for regular compiler/linker errors
        linker_match = self.linker_pattern.match(line)
        if linker_match:
            groups = linker_match.groupdict()
            self.errors.append(
                BuildError(
                    file=groups["file"],
                    line=int(groups["line"]),
                    col=int(groups["col"]),
                    type=groups["type"],
                    message=groups["message"],
                )
            )

    def print_summary(self) -> None:
        if not self.errors:
            print(colored("✓ Build successful - no errors found", "green"))
            return

        print("\n=== Build Error Summary ===\n")

        # Group errors by file
        errors_by_file: Dict[str, List[BuildError]] = {}
        for error in self.errors:
            if error.file not in errors_by_file:
                errors_by_file[error.file] = []
            errors_by_file[error.file].append(error)

        # Print errors grouped by file
        for file, errors in errors_by_file.items():
            print(colored(f"\nFile: {file}", "cyan"))
            for error in errors:
                color = "red" if error.type == "error" else "yellow"
                if error.symbol:  # Multiple definition error
                    print(
                        colored(f"  ↳ Multiple definition of '{error.symbol}'", color)
                    )
                    print(f"    First defined in: {error.first_defined}")
                else:  # Regular error
                    print(
                        colored(
                            f"  ↳ {error.type.upper()} at line {error.line}:", color
                        )
                    )
                    print(f"    {error.message}")

        # Print statistics
        total_errors = len([e for e in self.errors if e.type == "error"])
        total_warnings = len([e for e in self.errors if e.type == "warning"])
        print(
            f"\nTotal: {colored(f'{total_errors} errors', 'red')}, "
            f"{colored(f'{total_warnings} warnings', 'yellow')}"
        )


def main():
    parser = ErrorParser()

    # Read from stdin if no file provided
    for line in sys.stdin:
        parser.parse_line(line.strip())

    parser.print_summary()


if __name__ == "__main__":
    main()
