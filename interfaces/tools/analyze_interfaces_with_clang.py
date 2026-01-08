#!/usr/bin/env python3
"""
Analyze C++ interface headers using regex parsing.

Generates:
- Class hierarchy diagram (Mermaid)
- Interface dependency graph
- Method signatures summary

Uses regex parsing (most reliable for header-only interfaces).
"""

import sys
import json
import re
from pathlib import Path
from typing import Dict, List, Set

repo_root = Path(__file__).parent.parent.parent
interfaces_dir = repo_root / 'interfaces/include/next'

def extract_manually_from_headers(headers: List[Path]) -> Dict:
    """Extract class information using regex parsing (most reliable for headers)."""
    classes = {}

    for header in headers:
        content = header.read_text()

        # Extract class name (handle namespace and inheritance)
        class_match = re.search(r'class\s+(\w+)(?:\s*:\s*[^{]*)?\s*\{', content, re.MULTILINE)
        if not class_match:
            continue

        class_name = class_match.group(1)
        if not class_name.startswith('I'):
            continue

        classes[class_name] = {
            'name': class_name,
            'methods': [],
            'base_classes': [],
        }

        # Extract base classes (handle public/protected/private inheritance)
        base_pattern = r'class\s+\w+\s*:\s*(?:public|protected|private)\s+(\w+)'
        base_matches = re.findall(base_pattern, content)
        for base in base_matches:
            if base.startswith('I') and base not in classes[class_name]['base_classes']:
                classes[class_name]['base_classes'].append(base)

        # Extract method names - comprehensive patterns
        method_patterns = [
            r'virtual\s+[^;]*?\s+(\w+)\s*\([^)]*\)',  # virtual methods with return type
            r'virtual\s+~(\w+)\(\)',  # virtual destructor
            r'(?:virtual\s+)?\w+\s+(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?(?:=\s*0)?\s*;',  # any method declaration
        ]

        # Keywords and false positives to exclude
        excluded = {
            'virtual', 'const', 'override', 'final', 'default', 'delete',
            'operator', 'if', 'for', 'while', 'return', 'throw', 'new', 'delete',
            'public', 'private', 'protected', 'namespace', 'using', 'typedef',
            'template', 'typename', 'class', 'struct', 'enum', 'union',
            'static', 'inline', 'extern', 'friend', 'explicit', 'mutable',
            'true', 'false', 'nullptr', 'null', 'void', 'int', 'double', 'float',
            'bool', 'char', 'size_t', 'auto', 'decltype',
            'std', 'max', 'min', 'abs', 'sqrt', 'pow', 'sin', 'cos',
            'empty', 'size', 'begin', 'end', 'front', 'back',
            'clone', 'copy', 'move', 'swap', 'clear', 'resize',
            'infinity', 'numeric_limits', 'limits',
        }

        found_methods = set()
        for pattern in method_patterns:
            matches = re.findall(pattern, content, re.MULTILINE)
            for method in matches:
                if method and len(method) > 1 and method not in excluded:
                    if not method.startswith('operator'):
                        if method[0].islower() or (method[0].isupper() and len(method) > 3):
                            found_methods.add(method)

        classes[class_name]['methods'] = sorted(list(found_methods))

    return classes

def generate_mermaid_diagram(classes: Dict) -> str:
    """Generate Mermaid class diagram."""
    lines = [
        "```mermaid",
        "classDiagram",
        ""
    ]

    # Add classes
    for class_name, info in sorted(classes.items()):
        lines.append(f"    class {class_name} {{")
        for method in info['methods'][:6]:  # Limit to first 6 methods
            lines.append(f"        +{method}()")
        if len(info['methods']) > 6:
            lines.append(f"        ...")
        lines.append(f"    }}")
        lines.append("")

    # Add inheritance relationships
    for class_name, info in sorted(classes.items()):
        for base in info['base_classes']:
            if base in classes:
                lines.append(f"    {base} <|-- {class_name}")

    lines.append("```")
    return "\n".join(lines)

def analyze_interfaces():
    """Main analysis function."""
    print("Using regex parsing (most reliable for header-only interfaces)...")

    # Find all interface headers
    if not interfaces_dir.exists():
        print(f"ERROR: {interfaces_dir} does not exist")
        return None

    headers = sorted(interfaces_dir.glob('I*.h'))
    print(f"Found {len(headers)} interface headers")

    # Use manual regex parsing - most reliable for headers
    all_classes = extract_manually_from_headers(headers)

    print(f"\nFound {len(all_classes)} classes total:")
    for name, info in all_classes.items():
        print(f"  {name}: {len(info['methods'])} methods, {len(info['base_classes'])} bases")
        if info['methods']:
            print(f"    Methods: {', '.join(info['methods'][:5])}")
            if len(info['methods']) > 5:
                print(f"      ... and {len(info['methods']) - 5} more")

    return all_classes

def main():
    classes = analyze_interfaces()

    if not classes:
        print("ERROR: Could not extract interface information")
        sys.exit(1)

    print(f"\n=== Final Results ===")
    for name, info in sorted(classes.items()):
        print(f"\n{name}:")
        if info['base_classes']:
            print(f"  Base classes: {', '.join(info['base_classes'])}")
        print(f"  Methods ({len(info['methods'])}): {', '.join(info['methods'][:8])}")
        if len(info['methods']) > 8:
            print(f"    ... and {len(info['methods']) - 8} more")

    # Generate Mermaid diagram
    mermaid = generate_mermaid_diagram(classes)

    output_file = repo_root / 'interfaces/INTERFACE_DIAGRAM.md'
    output_file.write_text(f"# Interface Class Hierarchy\n\nGenerated from regex parsing.\n\n{mermaid}\n")
    print(f"\nGenerated diagram: {output_file}")

    # Generate JSON summary
    json_file = repo_root / 'interfaces/interface_summary.json'
    json_file.write_text(json.dumps(classes, indent=2))
    print(f"Generated JSON summary: {json_file}")

if __name__ == '__main__':
    main()