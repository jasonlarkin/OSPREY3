# Code Analysis Tools - Installation Status

## Installed Tools

### Static Analysis

**cppcheck 2.7**
- Static code analysis for C/C++
- Usage: `cppcheck --enable=all src/main/cc/`
- Location: `/usr/bin/cppcheck`

**cloc 1.90**
- Count lines of code
- Usage: `cloc src/main/cc/`
- Location: `/usr/bin/cloc`

**lizard 1.19.0**
- Cyclomatic complexity analyzer
- Usage: `python3 -m lizard src/main/cc/ -l cpp`
- Location: User site-packages (pip install --user)

---

## Quick Usage Examples

### Analyze C++ Code Complexity
```bash
cd osprey-fork
python3 -m lizard src/main/cc/ -l cpp
```

### Count Lines of Code
```bash
cloc src/main/cc/
```

### Run Static Analysis
```bash
cppcheck --enable=all src/main/cc/ConfEcalc/
```

### Generate Complexity Report
```bash
python3 -m lizard src/main/cc/ -l cpp --html > complexity-report.html
```

---

## Documentation

See `CODE_ANALYSIS_TOOLS.md` for comprehensive documentation on:
- All available tools
- Usage examples
- CI/CD integration
- Performance profiling tools

---

*Installed: 2025-01-XX*

