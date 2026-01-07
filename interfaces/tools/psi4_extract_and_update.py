#!/usr/bin/env python3
"""
Extract Psi4 wavefunction/energy interfaces from local headers and update interface inventory.

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
PSI4_HEADERS = {
    'Wavefunction': 'interfaces/psi4/psi4/src/psi4/libmints/wavefunction.h',
    'BasisSet': 'interfaces/psi4/psi4/src/psi4/libmints/basisset.h',
}

TARGET_DOSSIER = repo_root / 'interfaces/psi4/interface_inventory.md'


def extract_class_signatures(header_path: Path) -> Dict[str, List[str]]:
    """Extract public class methods and key declarations from a header."""
    if not header_path.exists():
        return {}
    
    content = header_path.read_text()
    
    signatures = []
    
    # Extract class definitions - handle PSI_API, macros, inheritance
    class_match = re.search(r'class\s+(?:PSI_API\s+)?(\w+)\s*(?:final)?\s*(?::\s*public\s+[^{]+)?\s*\{', content)
    if not class_match:
        return {}
    
    class_name = class_match.group(1)
    
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
    
    # Critical methods to extract (key interfaces for energy evaluation)
    critical_methods = {'compute_energy', 'compute_gradient', 'compute_hessian', 'molecule', 'basisset'}
    
    for section in public_sections:
        public_block = section.group(1)
        
        # Extract method declarations - handle both inline and non-inline
        lines = public_block.split('\n')
        seen = set()
        for i, line in enumerate(lines):
            line_stripped = line.strip()
            # Skip empty lines, comments, control flow
            if not line_stripped or line_stripped.startswith('//') or line_stripped.startswith('/*') or line_stripped.startswith('*') or line_stripped.startswith('///'):
                continue
            if line_stripped.startswith('if ') or line_stripped.startswith('for ') or line_stripped.startswith('while ') or line_stripped.startswith('throw ') or line_stripped.startswith('return '):
                continue
            
            # Match method signatures: [virtual] return_type method_name(params) [const] [override] [= 0] [; | {]
            # Pattern needs to capture full signature including return type (handle SharedMatrix, etc.)
            # Match patterns like: "virtual SharedMatrix compute_gradient() {" or "virtual double compute_energy() {"
            method_match = re.match(r'^\s*(?:virtual\s+)?((?:[:\w<>,\s&*]+\s+)?~?(\w+)\s*\([^)]*\)(?:\s*const)?(?:\s*override)?(?:\s*=\s*0)?)\s*[;{]', line_stripped)
            if method_match:
                full_sig = method_match.group(1)
                method_name = method_match.group(2)
                sig_clean = re.sub(r'\s+', ' ', full_sig.strip())
                sig_clean = re.sub(r'//.*$', '', sig_clean).strip()
                
                # Prioritize critical methods, but capture all to avoid missing important ones
                is_critical = method_name in critical_methods or 'compute' in method_name or method_name in {'Energy', 'Setup', 'GetCoordinates', 'SetCoordinates'}
                
                if sig_clean and '(' in sig_clean and len(sig_clean) > 3:
                    # Extract return type separately if it's a complex type
                    if 'SharedMatrix' in sig_clean or 'SharedVector' in sig_clean:
                        # Keep full signature including return type
                        pass
                    if sig_clean not in seen:
                        signatures.append(sig_clean)
                        seen.add(sig_clean)
    
    # Also look for key typedefs, static methods
    typedefs = re.findall(r'typedef\s+.*?(\w+)\s*;', class_body)
    static_methods = re.findall(r'static\s+[^{]+?(\w+\s*\([^)]*\))\s*;', class_body)
    
    return {
        'class_name': class_name,
        'methods': signatures[:30],
        'typedefs': typedefs[:10],
        'static_methods': static_methods[:10],
    }


def extract_all_interfaces() -> Dict[str, Dict]:
    """Extract interfaces from all target headers."""
    results = {}
    
    for interface_name, rel_path in PSI4_HEADERS.items():
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
        "<!-- AUTO:PSI4_API_START -->",
        "",
        "## Auto-extracted Psi4 interfaces (from local headers)",
        "",
        "Generated by `interfaces/tools/psi4_extract_and_update.py`.",
        "",
    ]
    
    for name, data in extracted.items():
        if not data or 'class_name' not in data:
            continue
            
        lines.append(f"### `{data['class_name']}` ({name})")
        lines.append("")
        
        if data.get('methods'):
            for method in data['methods']:
                if method and len(method) < 250:  # Skip very long lines
                    lines.append(f"- `{method}`")
            lines.append("")
        
        if data.get('typedefs'):
            lines.append("**Typedefs:**")
            for td in data['typedefs']:
                lines.append(f"- `{td}`")
            lines.append("")
    
    lines.append("<!-- AUTO:PSI4_API_END -->")
    
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
    print("Extracting Psi4 interfaces...")
    extracted = extract_all_interfaces()
    
    formatted = format_extracted_api(extracted)
    
    if '--update-dossier' in sys.argv:
        update_dossier("<!-- AUTO:PSI4_API_START -->", formatted)
    else:
        print("\n" + "="*80)
        print(formatted)
        print("\n(Use --update-dossier to write to dossier)")


if __name__ == '__main__':
    main()
