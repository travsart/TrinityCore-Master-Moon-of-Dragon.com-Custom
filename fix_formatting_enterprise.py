#!/usr/bin/env python3
"""
Enterprise-Grade Formatting Fix Script for Playerbot Source Files

This script fixes specific formatting corruption patterns identified in the codebase:
1. Function definitions with '{' on same line: `void Func(params){` -> `void Func(params)\n{`
2. Multiple statements on same line: `code1;    code2` -> `code1;\n    code2`
3. Closing brace followed by code: `}if` -> `}\nif`

SAFETY FEATURES:
- Dry-run mode (default): Shows changes without applying
- Single-file mode: Test on one file before mass application
- Detailed diff output: See exactly what will change
- Line-by-line processing: Predictable, debuggable behavior
- Conservative matching: Only fix clearly corrupted patterns

Usage:
    python fix_formatting_enterprise.py --dry-run                    # Show all changes without applying
    python fix_formatting_enterprise.py --dry-run --file <path>      # Test on single file
    python fix_formatting_enterprise.py --apply --file <path>        # Apply to single file
    python fix_formatting_enterprise.py --apply                      # Apply to all files (use after validation)
"""

import os
import re
import sys
import argparse
import io
from typing import List, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

# Fix Windows console UTF-8 output
if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')


class FixType(Enum):
    """Types of fixes applied."""
    FUNC_BRACE_SAME_LINE = "Function brace on same line"
    MULTI_STATEMENT_LINE = "Multiple statements on line"
    CLOSING_BRACE_CODE = "Closing brace followed by code"


@dataclass
class Fix:
    """Represents a single fix to be applied."""
    line_number: int
    fix_type: FixType
    original_line: str
    fixed_lines: List[str]
    description: str


class FormattingFixer:
    """Enterprise-grade formatting fixer with safety features."""

    def __init__(self, dry_run: bool = True, verbose: bool = True):
        self.dry_run = dry_run
        self.verbose = verbose
        self.total_fixes = 0
        self.files_modified = 0
        self.files_processed = 0

    def analyze_file(self, filepath: str) -> Tuple[List[str], List[Fix]]:
        """
        Analyze a file and return the fixed content with a list of fixes.

        Returns:
            Tuple of (fixed_lines, list_of_fixes)
        """
        try:
            with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
                lines = f.readlines()
        except Exception as e:
            print(f"ERROR: Cannot read {filepath}: {e}")
            return [], []

        fixes: List[Fix] = []
        result_lines: List[str] = []

        i = 0
        while i < len(lines):
            line = lines[i]
            line_num = i + 1  # 1-indexed for user display
            original_line = line

            # Track if we need to process this line specially
            processed = False

            # ================================================================
            # PATTERN 1: Function definition with '{' on same line
            # Match: `void Func(params){` or `) const{` or `) override{`
            # But NOT: lambdas like `[](int x){ return x; }`
            # But NOT: struct initializers like `= {`
            # ================================================================
            if not processed:
                # Pattern: end of function signature with { attached
                # Must have ) followed by optional const/override/final then {
                func_brace_pattern = r'^([^=]*\))\s*(const|override|final|noexcept)?\s*\{\s*$'
                match = re.match(func_brace_pattern, line.rstrip('\n\r'))

                if match:
                    # Don't fix if it's a lambda (has [ before the ))
                    if '[' not in line or line.find('[') > line.find(')'):
                        # Don't fix if it's an inline single-line function (rare, but check)
                        # Get indentation
                        indent_match = re.match(r'^(\s*)', line)
                        indent = indent_match.group(1) if indent_match else ''

                        # Build fixed version
                        prefix = match.group(1)
                        modifier = match.group(2) or ''

                        if modifier:
                            fixed_line1 = f"{prefix} {modifier}\n"
                        else:
                            fixed_line1 = f"{prefix}\n"
                        fixed_line2 = f"{indent}{{\n"

                        fixes.append(Fix(
                            line_number=line_num,
                            fix_type=FixType.FUNC_BRACE_SAME_LINE,
                            original_line=line,
                            fixed_lines=[fixed_line1, fixed_line2],
                            description=f"Split function brace to new line"
                        ))

                        result_lines.append(fixed_line1)
                        result_lines.append(fixed_line2)
                        processed = True

            # ================================================================
            # PATTERN 2: Multiple statements on same line
            # Match: `code1;    code2` (semicolon + 4+ spaces + identifier)
            # But NOT: for loops (which have multiple ; legitimately)
            # But NOT: comments (// or /* after ;)
            # But NOT: string literals
            # ================================================================
            if not processed:
                # Skip for loops
                stripped = line.strip()
                if not stripped.startswith('for') and not stripped.startswith('//') and not stripped.startswith('#'):
                    # Look for pattern: ;    [identifier or keyword]
                    # Use regex to find ; followed by 4+ spaces then a word char
                    multi_stmt_pattern = r';(\s{4,})([a-zA-Z_])'
                    match = re.search(multi_stmt_pattern, line)

                    if match:
                        # Check if the ; is inside a string or comment
                        semicolon_pos = match.start()
                        before_semicolon = line[:semicolon_pos]

                        # Simple check: count quotes before semicolon (odd = inside string)
                        # Also check for // comment before semicolon
                        in_string = before_semicolon.count('"') % 2 == 1
                        in_comment = '//' in before_semicolon

                        if not in_string and not in_comment:
                            # Get indentation of the original line
                            indent_match = re.match(r'^(\s*)', line)
                            indent = indent_match.group(1) if indent_match else ''

                            # Split the line
                            first_part = line[:semicolon_pos + 1].rstrip() + '\n'
                            second_part = indent + line[match.end() - 1:]

                            fixes.append(Fix(
                                line_number=line_num,
                                fix_type=FixType.MULTI_STATEMENT_LINE,
                                original_line=line,
                                fixed_lines=[first_part, second_part],
                                description=f"Split multiple statements to separate lines"
                            ))

                            result_lines.append(first_part)
                            result_lines.append(second_part)
                            processed = True

            # ================================================================
            # PATTERN 3: Closing brace followed immediately by code
            # Match: `}if`, `}else`, `}//`, `}TC_LOG` at start of line
            # But NOT: `};` (valid end of struct/class)
            # But NOT: `},` (valid in initializer lists)
            # But NOT: `{}` format specifiers inside strings
            # Only match when } is at start of line (with optional whitespace)
            # ================================================================
            if not processed:
                # More conservative: only match } at line start followed by identifier
                close_brace_pattern = r'^(\s*)\}([a-zA-Z_/])'
                match = re.match(close_brace_pattern, line)

                if match:
                    indent = match.group(1)
                    next_char = match.group(2)

                    # Get the rest of the line after }
                    rest_of_line = line[len(indent) + 1:]

                    # Split into two lines
                    first_part = indent + '}\n'
                    second_part = indent + rest_of_line

                    fixes.append(Fix(
                        line_number=line_num,
                        fix_type=FixType.CLOSING_BRACE_CODE,
                        original_line=line,
                        fixed_lines=[first_part, second_part],
                        description=f"Add newline after closing brace"
                    ))

                    result_lines.append(first_part)
                    result_lines.append(second_part)
                    processed = True

            # ================================================================
            # PATTERN 4: Code ending with `;}` or `break;}` (code then closing brace)
            # Match: `break;}` or `return x;}`
            # Split: `break;\n}`
            # ================================================================
            if not processed:
                # Look for pattern: statement;}$ at end of line
                end_brace_pattern = r'^(\s*)(.+;)\}(\s*)$'
                match = re.match(end_brace_pattern, line)

                if match:
                    indent = match.group(1)
                    statement = match.group(2)
                    trailing = match.group(3)

                    # Don't process if this looks like a lambda or inline function
                    # Check if there's a { earlier on the same line (inline function)
                    if '{' not in statement:
                        # Split into statement and closing brace
                        first_part = indent + statement + '\n'
                        second_part = indent + '}\n'

                        fixes.append(Fix(
                            line_number=line_num,
                            fix_type=FixType.CLOSING_BRACE_CODE,
                            original_line=line,
                            fixed_lines=[first_part, second_part],
                            description=f"Split statement from closing brace"
                        ))

                        result_lines.append(first_part)
                        result_lines.append(second_part)
                        processed = True

            # ================================================================
            # PATTERN 5: `);if` or `);else` - code on same line after )
            # Match: `... GetCounter());if (result)`
            # Split: `... GetCounter());\nif (result)`
            # ================================================================
            if not processed:
                # Look for );[identifier] pattern (not at line start)
                post_paren_pattern = r'^(.+\);)([a-zA-Z_]\S*.*)$'
                match = re.match(post_paren_pattern, line)

                if match:
                    first_half = match.group(1)
                    second_half = match.group(2)

                    # Don't fix if it's clearly inside a string
                    # Simple heuristic: count quotes
                    if first_half.count('"') % 2 == 0:
                        # Get base indentation
                        indent_match = re.match(r'^(\s*)', line)
                        indent = indent_match.group(1) if indent_match else ''

                        first_part = first_half + '\n'
                        second_part = indent + second_half

                        fixes.append(Fix(
                            line_number=line_num,
                            fix_type=FixType.CLOSING_BRACE_CODE,
                            original_line=line,
                            fixed_lines=[first_part, second_part],
                            description=f"Split code after closing paren/semicolon"
                        ))

                        result_lines.append(first_part)
                        result_lines.append(second_part)
                        processed = True

            # If no fix needed, keep original line
            if not processed:
                result_lines.append(line)

            i += 1

        return result_lines, fixes

    def process_file(self, filepath: str) -> bool:
        """
        Process a single file. Returns True if changes were made/would be made.
        """
        self.files_processed += 1

        fixed_lines, fixes = self.analyze_file(filepath)

        if not fixes:
            return False

        self.total_fixes += len(fixes)
        self.files_modified += 1

        if self.verbose:
            print(f"\n{'='*60}")
            print(f"FILE: {filepath}")
            print(f"{'='*60}")

            for fix in fixes:
                print(f"\nLine {fix.line_number}: {fix.fix_type.value}")
                print(f"  BEFORE: {fix.original_line.rstrip()}")
                print(f"  AFTER:")
                for fl in fix.fixed_lines:
                    print(f"    {fl.rstrip()}")

        if not self.dry_run:
            try:
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.writelines(fixed_lines)
                if self.verbose:
                    print(f"  [APPLIED]")
            except Exception as e:
                print(f"ERROR: Cannot write {filepath}: {e}")
                return False
        else:
            if self.verbose:
                print(f"  [DRY RUN - not applied]")

        return True

    def process_directory(self, directory: str) -> None:
        """Process all .cpp and .h files in directory recursively."""
        for root, dirs, files in os.walk(directory):
            # Skip deps directory
            if 'deps' in root:
                continue

            for filename in files:
                if filename.endswith(('.cpp', '.h')):
                    filepath = os.path.join(root, filename)
                    self.process_file(filepath)

    def print_summary(self) -> None:
        """Print summary of all fixes."""
        print(f"\n{'='*60}")
        print("SUMMARY")
        print(f"{'='*60}")
        print(f"Files processed: {self.files_processed}")
        print(f"Files with changes: {self.files_modified}")
        print(f"Total fixes: {self.total_fixes}")
        if self.dry_run:
            print(f"\nDRY RUN MODE - No changes were applied")
            print(f"Run with --apply to apply changes")


def main():
    parser = argparse.ArgumentParser(
        description='Enterprise-grade formatting fix for Playerbot source files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )

    parser.add_argument(
        '--dry-run',
        action='store_true',
        default=True,
        help='Show changes without applying them (default)'
    )

    parser.add_argument(
        '--apply',
        action='store_true',
        help='Actually apply the changes'
    )

    parser.add_argument(
        '--file',
        type=str,
        help='Process only this specific file'
    )

    parser.add_argument(
        '--quiet',
        action='store_true',
        help='Only show summary, not individual fixes'
    )

    parser.add_argument(
        '--directory',
        type=str,
        default=r'C:\TrinityBots\TrinityCore\src\modules\Playerbot',
        help='Directory to process (default: Playerbot module)'
    )

    args = parser.parse_args()

    # --apply overrides --dry-run
    dry_run = not args.apply
    verbose = not args.quiet

    print(f"Formatting Fixer - {'DRY RUN' if dry_run else 'APPLY MODE'}")
    print(f"{'='*60}")

    fixer = FormattingFixer(dry_run=dry_run, verbose=verbose)

    if args.file:
        if not os.path.exists(args.file):
            print(f"ERROR: File not found: {args.file}")
            sys.exit(1)
        fixer.process_file(args.file)
    else:
        if not os.path.exists(args.directory):
            print(f"ERROR: Directory not found: {args.directory}")
            sys.exit(1)
        fixer.process_directory(args.directory)

    fixer.print_summary()


if __name__ == '__main__':
    main()
