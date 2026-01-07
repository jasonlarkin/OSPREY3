#!/usr/bin/env python3
"""
Extract Open Babel forcefield/conformer search interfaces from local headers and update interface inventory.

One-shot script: extracts key interface signatures and updates the dossier.
"""

import re
import sys
from pathlib import Path
from typing import List, Dict

# Add repo root to path for imports if needed
repo_root = Path(__file__).parent.parent.parent
sys.path.insert(0, str(repo_root))

# Key headers to extract from
OBABEL_HEADERS = {
    'OBForceField': 'interfaces/openbabel/include/openbabel/forcefield.h',
    'OBConformerSearch': 'interfaces/openbabel/include/openbabel/conformersearch.h',
}

TARGET_DOSSIER = repo_root / 'interfaces/openbabel/interface_inventory.md'


def extract_class_signatures(header_path: Path) -> Dict[str, List[str]]:
    """Extract public class methods and key declarations from a header."""
    if not header_path.exists():
        return {}
    
    content = header_path.read_text()
    
    signatures = []
    
    # Extract specific class by name (OBForceField or OBConformerSearch)
    target_class = None
    if 'forcefield.h' in str(header_path):
        target_class = 'OBForceField'
    elif 'conformersearch.h' in str(header_path):
        target_class = 'OBConformerSearch'
    
    if not target_class:
        return {}
    
    # Find the target class - handle OBAPI, OBFPRT macros, inheritance
    pattern1 = r'class\s+(?:OBAPI|OBFPRT)\s+' + target_class + r'\s*(?::\s*public\s+[^{]+)?\s*\{'
    class_match = re.search(pattern1, content)
    if not class_match:
        pattern2 = r'class\s+' + target_class + r'\s*(?::\s*public\s+[^{]+)?\s*\{'
        class_match = re.search(pattern2, content)
    if not class_match:
        return {}
    
    class_name = target_class
    
    # Find the class body boundaries
    class_start = class_match.end() - 1
    brace_count = 1
    class_end = class_start + 1
    while brace_count > 0 and class_end < len(content):
        if content[class_end] == '{':
            brace_count += 1
        elif content[class_end] == '}':
            brace_count -= 1
        class_end += 1
    
    class_body = content[class_start:class_end]
    
    # Extract public: sections
    public_sections = re.finditer(r'public:\s*\n((?:[^}]*\n)*?)(?=\s*(?:private:|protected:|public:|}))', class_body, re.MULTILINE)
    
    for section in public_sections:
        public_block = section.group(1)
        
        # Extract method declarations - focus on key interfaces: Energy, Setup, GetCoordinates, etc.
        critical_methods = {'Energy', 'Setup', 'GetCoordinates', 'SetCoordinates', 'GetConformers', 'SetConformers', 'SteepestDescent', 'ConjugateGradients'}
        
        lines = public_block.split('\n')
        seen = set()
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            # Skip empty lines, comments, control flow
            if not line_stripped or line_stripped.startswith('//') or line_stripped.startswith('/*') or line_stripped.startswith('*') or line_stripped.startswith('//!'):
                continue
            if line_stripped.startswith('if ') or line_stripped.startswith('for ') or line_stripped.startswith('while ') or line_stripped.startswith('return ') or line_stripped.startswith('throw '):
                continue
            
            # Match method signatures: [virtual] [inline] return_type method_name(params) [const] [override] [= 0] [; | {]
            # Handle macros like UNUSED(...) in params - use simpler pattern that handles nested parens
            method_match = re.match(r'^\s*(?:virtual\s+)?(?:inline\s+)?((?:[:\w<>,\s&*]+\s+)?~?(\w+)\s*\([^)]*\)(?:\s*const)?(?:\s*override)?(?:\s*=\s*0)?)\s*[;{]', line_stripped)
            if method_match:
                full_sig = method_match.group(1)
                method_name = method_match.group(2)
                sig_clean = re.sub(r'\s+', ' ', full_sig.strip())
                sig_clean = re.sub(r'//.*$', '', sig_clean).strip()
                
                # Prioritize critical interface methods
                is_critical = method_name in critical_methods or method_name.startswith('E_') or 'Energy' in method_name
                
                if sig_clean and '(' in sig_clean and len(sig_clean) > 3 and sig_clean not in seen:
                    signatures.append(sig_clean)
                    seen.add(sig_clean)
    
    # Also look for key typedefs, static methods
    typedefs = re.findall(r'typedef\s+.*?(\w+)\s*;', class_body)
    static_methods = re.findall(r'static\s+[^{]+?(\w+\s*\([^)]*\))\s*;', class_body)
    
    # Extract energy term flags from entire file
    energy_flags = re.findall(r'#define\s+(OBFF_E\w+)', content)
    
    return {
        'class_name': class_name,
        'methods': signatures[:30],
        'typedefs': typedefs[:10],
        'static_methods': static_methods[:10],
        'energy_flags': energy_flags,
    }


def extract_all_interfaces() -> Dict[str, Dict]:
    """Extract interfaces from all target headers."""
    results = {}
    
    for interface_name, rel_path in OBABEL_HEADERS.items():
        full_path = repo_root / rel_path
        if full_path.exists():
            print(f"Extracting from {full_path}...")
            results[interface_name] = extract_class_signatures(full_path)
        else:
            print(f"Warning: {full_path} not found")
    
    return results


def format_extracted_api(extracted: Dict[str, Dict]) -> str:
    """Format extracted API into markdown."""
    lines = [
        "<!-- AUTO:OBABEL_API_START -->",
        "",
        "## Auto-extracted Open Babel interfaces (from local headers)",
        "",
        "Generated by `interfaces/tools/openbabel_extract_and_update.py`.",
        "",
    ]
    
    for name, data in extracted.items():
        if not data or 'class_name' not in data:
            continue
            
        lines.append(f"### `{data['class_name']}` ({name})")
        lines.append("")
        
        if data.get('methods'):
            for method in data['methods']:
                if method and len(method) < 250:
                    lines.append(f"- `{method}`")
            lines.append("")
        
        if data.get('energy_flags'):
            lines.append("**Energy term flags:**")
            for flag in data['energy_flags']:
                lines.append(f"- `{flag}`")
            lines.append("")
        
        if data.get('typedefs'):
            lines.append("**Typedefs:**")
            for td in data['typedefs']:
                lines.append(f"- `{td}`")
            lines.append("")
    
    lines.append("<!-- AUTO:OBABEL_API_END -->")
    
    return "\n".join(lines)


def update_dossier(content_marker: str, new_content: str):
    """Update the dossier file between AUTO markers."""
    if not TARGET_DOSSIER.exists():
        # Create new file
        TARGET_DOSSIER.parent.mkdir(parents=True, exist_ok=True)
        TARGET_DOSSIER.write_text(new_content + "\n")
        print(f"Created new dossier at {TARGET_DOSSIER}")
        return
    
    content = TARGET_DOSSIER.read_text()
    
    # Find and replace between markers
    pattern = rf'({re.escape(content_marker)}.*?){re.escape(content_marker.replace("START", "END"))}'
    
    if re.search(pattern, content, re.DOTALL):
        # Replace existing section
        new_file_content = re.sub(pattern, new_content, content, flags=re.DOTALL)
        TARGET_DOSSIER.write_text(new_file_content)
        print(f"Updated existing AUTO section in {TARGET_DOSSIER}")
    else:
        # Append new section
        TARGET_DOSSIER.write_text(content + "\n\n" + new_content + "\n")
        print(f"Appended new AUTO section to {TARGET_DOSSIER}")


def main():
    print("Extracting Open Babel interfaces...")
    extracted = extract_all_interfaces()
    
    formatted = format_extracted_api(extracted)
    
    if '--update-dossier' in sys.argv:
        update_dossier("<!-- AUTO:OBABEL_API_START -->", formatted)
    else:
        print("\n" + "="*80)
        print(formatted)
        print("\n(Use --update-dossier to write to dossier)")


if __name__ == '__main__':
    main()
