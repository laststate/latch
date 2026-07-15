import argparse
import json
import shutil
import sys
from pathlib import Path


SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".rs",
    ".s",
}
EXCLUDED_DIRECTORIES = {".git", ".vs", "build", "target", "__pycache__", "_CPack_Packages"}
PUNCTUATORS = tuple(
    sorted(
        {
            "%:%:",
            ">>=",
            "<<=",
            "->*",
            "...",
            "##",
            "::",
            ".*",
            "->",
            "++",
            "--",
            "<<",
            ">>",
            "<=",
            ">=",
            "==",
            "!=",
            "&&",
            "||",
            "*=",
            "/=",
            "%=",
            "+=",
            "-=",
            "&=",
            "^=",
            "|=",
            "<:",
            ":>",
            "<%",
            "%>",
            "%:",
        },
        key=len,
        reverse=True,
    )
)
RUST_PUNCTUATORS = tuple(sorted(set(PUNCTUATORS) | {"..=", "..", "=>"}, key=len, reverse=True))


def is_identifier_character(character):
    return character == "_" or character.isalnum()


def read_quoted(source, index):
    quote = source[index]
    cursor = index + 1
    while cursor < len(source):
        character = source[cursor]
        if character == "\\":
            cursor += 2
            continue
        cursor += 1
        if character == quote:
            return source[index:cursor], cursor
    raise ValueError("unterminated string or character literal")


def read_raw_string(source, index):
    opening = source.find("(", index + 2)
    if opening == -1:
        raise ValueError("invalid raw string literal")
    delimiter = source[index + 2 : opening]
    closing = ")" + delimiter + '"'
    end = source.find(closing, opening + 1)
    if end == -1:
        raise ValueError("unterminated raw string literal")
    end += len(closing)
    return source[index:end], end


def read_number(source, index):
    cursor = index
    while cursor < len(source) and (source[cursor].isalnum() or source[cursor] in "._"):
        cursor += 1
    return source[index:cursor], cursor


def read_identifier(source, index):
    cursor = index + 1
    while cursor < len(source) and is_identifier_character(source[cursor]):
        cursor += 1
    return source[index:cursor], cursor


def read_punctuator(source, index):
    for punctuator in PUNCTUATORS:
        if source.startswith(punctuator, index):
            return punctuator, index + len(punctuator)
    return source[index], index + 1


def read_rust_punctuator(source, index):
    for punctuator in RUST_PUNCTUATORS:
        if source.startswith(punctuator, index):
            return punctuator, index + len(punctuator)
    return source[index], index + 1


def read_token(source, index):
    character = source[index]
    if character in "\"'":
        return read_quoted(source, index)
    if character == "R" and source.startswith('R"', index):
        return read_raw_string(source, index)
    if is_identifier_character(character):
        return read_identifier(source, index)
    if character.isdigit() or (character == "." and index + 1 < len(source) and source[index + 1].isdigit()):
        return read_number(source, index)
    return read_punctuator(source, index)


def tokenize_fragment(source):
    tokens = []
    cursor = 0
    while cursor < len(source):
        token, cursor = read_token(source, cursor)
        tokens.append(token)
    return tokens


def read_rust_raw_string(source, index):
    for prefix in ("br", "rb", "cr", "rc", "r"):
        if not source.startswith(prefix, index):
            continue
        cursor = index + len(prefix)
        while cursor < len(source) and source[cursor] == "#":
            cursor += 1
        if cursor >= len(source) or source[cursor] != '"':
            continue
        terminator = '"' + source[index + len(prefix) : cursor]
        end = source.find(terminator, cursor + 1)
        if end == -1:
            raise ValueError("unterminated Rust raw string literal")
        end += len(terminator)
        return source[index:end], end
    return None


def read_rust_character_or_lifetime(source, index):
    cursor = index + 1
    if cursor >= len(source) or source[cursor] == "\n":
        return "'", cursor
    if source[cursor] == "\\":
        cursor += 1
        if cursor >= len(source):
            raise ValueError("unterminated Rust character escape")
        if source[cursor] == "u":
            cursor += 1
            if cursor >= len(source) or source[cursor] != "{":
                return "'", index + 1
            closing = source.find("}", cursor + 1)
            if closing == -1:
                raise ValueError("unterminated Rust character escape")
            cursor = closing + 1
        elif source[cursor] == "x":
            cursor += 3
        else:
            cursor += 1
    else:
        cursor += 1
    if cursor < len(source) and source[cursor] == "'":
        cursor += 1
        return source[index:cursor], cursor
    return "'", index + 1


def read_rust_token(source, index):
    raw = read_rust_raw_string(source, index)
    if raw is not None:
        return raw
    character = source[index]
    if character == "'":
        return read_rust_character_or_lifetime(source, index)
    if character == '"':
        return read_quoted(source, index)
    if character in {"b", "c"} and index + 1 < len(source) and source[index + 1] == '"':
        literal, cursor = read_quoted(source, index + 1)
        return character + literal, cursor
    if is_identifier_character(character):
        return read_identifier(source, index)
    if character.isdigit() or (character == "." and index + 1 < len(source) and source[index + 1].isdigit()):
        return read_number(source, index)
    return read_rust_punctuator(source, index)


def tokenize_rust_fragment(source):
    tokens = []
    cursor = 0
    while cursor < len(source):
        token, cursor = read_rust_token(source, cursor)
        tokens.append(token)
    return tokens


def needs_separator(previous, current):
    if previous.endswith("/") and current.startswith(("/", "*")):
        return True
    return tokenize_fragment(previous + current) != [previous, current]


def needs_rust_separator(previous, current):
    if previous.endswith("/") and current.startswith(("/", "*")):
        return True
    if previous and is_identifier_character(previous[-1]) and current.startswith(('"', "'")):
        return True
    return tokenize_rust_fragment(previous + current) != [previous, current]


def read_directive(source, index):
    cursor = index
    while cursor < len(source):
        if source[cursor] == "\\" and cursor + 1 < len(source) and source[cursor + 1] == "\n":
            cursor += 2
            continue
        if source[cursor] == "\n":
            break
        cursor += 1
    return source[index:cursor].rstrip(), cursor


def minify_source(source):
    output = []
    cursor = 0
    previous = None
    at_line_start = True

    while cursor < len(source):
        character = source[cursor]
        if character.isspace():
            if character == "\n":
                at_line_start = True
            cursor += 1
            continue

        if at_line_start and character == "#":
            directive, cursor = read_directive(source, cursor)
            if output and not output[-1].endswith("\n"):
                output.append("\n")
            output.append(directive)
            output.append("\n")
            previous = None
            at_line_start = True
            if cursor < len(source) and source[cursor] == "\n":
                cursor += 1
            continue

        at_line_start = False
        if source.startswith("//", cursor):
            end = source.find("\n", cursor + 2)
            if end == -1:
                break
            cursor = end + 1
            at_line_start = True
            continue

        if source.startswith("/*", cursor):
            end = source.find("*/", cursor + 2)
            if end == -1:
                raise ValueError("unterminated block comment")
            at_line_start = "\n" in source[cursor:end + 2]
            cursor = end + 2
            continue

        token, cursor = read_token(source, cursor)
        if previous is not None and needs_separator(previous, token):
            output.append(" ")
        output.append(token)
        previous = token

    result = "".join(output).rstrip()
    return result + "\n" if result else ""


def skip_rust_block_comment(source, index):
    depth = 1
    cursor = index + 2
    while cursor < len(source):
        if source.startswith("/*", cursor):
            depth += 1
            cursor += 2
            continue
        if source.startswith("*/", cursor):
            depth -= 1
            cursor += 2
            if depth == 0:
                return cursor
            continue
        cursor += 1
    raise ValueError("unterminated Rust block comment")


def minify_rust_source(source):
    output = []
    cursor = 0
    previous = None
    while cursor < len(source):
        if source[cursor].isspace():
            cursor += 1
            continue
        if source.startswith("//", cursor):
            end = source.find("\n", cursor + 2)
            cursor = len(source) if end == -1 else end + 1
            continue
        if source.startswith("/*", cursor):
            cursor = skip_rust_block_comment(source, cursor)
            continue
        token, cursor = read_rust_token(source, cursor)
        if previous is not None and needs_rust_separator(previous, token):
            output.append(" ")
        output.append(token)
        previous = token
    result = "".join(output)
    return result + "\n" if result else ""


def minify_assembly_source(source):
    lines = []
    for line in source.splitlines():
        stripped = line.strip()
        if stripped:
            lines.append(" ".join(stripped.split()))
    return "\n".join(lines) + "\n" if lines else ""


def minifier_for_path(path):
    suffix = path.suffix.lower()
    if suffix == ".rs":
        return minify_rust_source
    if suffix == ".s":
        return minify_assembly_source
    return minify_source


def is_within(path, parent):
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def should_skip(path, source_root, output_root):
    relative = path.relative_to(source_root)
    if is_within(path, output_root):
        return True
    return any(part in EXCLUDED_DIRECTORIES or part.startswith("build-") for part in relative.parts)


def copy_project(source_root, output_root, verify):
    files = []
    original_size = 0
    minified_size = 0
    for path in source_root.rglob("*"):
        if should_skip(path, source_root, output_root) or not path.is_file():
            continue
        destination = output_root / path.relative_to(source_root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        original_size += path.stat().st_size
        if path.suffix.lower() not in SOURCE_SUFFIXES:
            shutil.copy2(path, destination)
            minified_size += destination.stat().st_size
            continue
        source = path.read_text(encoding="utf-8")
        minifier = minifier_for_path(path)
        minified = minifier(source)
        if verify and minifier(minified) != minified:
            raise ValueError(f"minifier is not idempotent for {path}")
        destination.write_text(minified, encoding="utf-8", newline="\n")
        minified_size += len(minified.encode("utf-8"))
        files.append(
            {
                "path": path.relative_to(source_root).as_posix(),
                "original_bytes": len(source.encode("utf-8")),
                "minified_bytes": len(minified.encode("utf-8")),
            }
        )
    return {"files": files, "original_bytes": original_size, "output_bytes": minified_size}


def minify_project_in_place(source_root, verify):
    files = []
    original_size = 0
    output_size = 0
    for path in source_root.rglob("*"):
        relative = path.relative_to(source_root)
        if not path.is_file() or any(
            part in EXCLUDED_DIRECTORIES or part.startswith("build-")
            for part in relative.parts
        ):
            continue
        if path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        source = path.read_text(encoding="utf-8")
        minifier = minifier_for_path(path)
        minified = minifier(source)
        if verify and minifier(minified) != minified:
            raise ValueError(f"minifier is not idempotent for {path}")
        path.write_text(minified, encoding="utf-8", newline="\n")
        original_size += len(source.encode("utf-8"))
        output_size += len(minified.encode("utf-8"))
        files.append(
            {
                "path": relative.as_posix(),
                "original_bytes": len(source.encode("utf-8")),
                "minified_bytes": len(minified.encode("utf-8")),
            }
        )
    return {"files": files, "original_bytes": original_size, "output_bytes": output_size}


def parse_arguments():
    parser = argparse.ArgumentParser(description="Create or apply a compact source representation.")
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--output", type=Path)
    target.add_argument("--in-place", action="store_true")
    parser.add_argument("--replace", action="store_true")
    parser.add_argument("--verify", action="store_true")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    source_root = arguments.source.resolve()
    if arguments.in_place:
        manifest = minify_project_in_place(source_root, arguments.verify)
        print(f"minified {len(manifest['files'])} source files in {source_root}")
        print(f"source files: {manifest['original_bytes']} bytes; output: {manifest['output_bytes']} bytes")
        return

    output_root = arguments.output.resolve()
    if source_root == output_root:
        raise ValueError("output must differ from source")
    if output_root.exists():
        if not arguments.replace:
            raise ValueError("output already exists; use --replace to recreate it")
        if not (output_root / "minify-manifest.json").is_file():
            raise ValueError("refusing to replace a directory not created by this tool")
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)
    manifest = copy_project(source_root, output_root, arguments.verify)
    (output_root / "minify-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(manifest['files'])} minified source files to {output_root}")
    print(f"source tree: {manifest['original_bytes']} bytes; output tree: {manifest['output_bytes']} bytes")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"minify_sources: {error}", file=sys.stderr)
        sys.exit(1)
