# GitHub Issues Search Results

This document summarizes the search for existing GitHub issues related to the serialization problems we identified and fixed.

## Search Methodology

Used GitHub CLI (`gh`) to search the [donaldlab/OSPREY3](https://github.com/donaldlab/OSPREY3) repository for issues related to:
- StackOverflowError
- NotSerializableException
- Serialization issues
- Amber forcefield files we modified

## Search Commands Executed

```bash
# Search for StackOverflowError issues
gh issue list --repo donaldlab/OSPREY3 --limit 100 \
  --search "StackOverflowError OR stackoverflow OR 'stack overflow'" \
  --state all

# Search for serialization issues
gh issue list --repo donaldlab/OSPREY3 --limit 100 \
  --search "NotSerializableException OR serialization OR Serializable OR deepCopy" \
  --state all

# Search for Amber forcefield file issues
gh issue list --repo donaldlab/OSPREY3 --limit 100 \
  --search "ForcefieldFileParser OR AtomSymbolAndMass OR ObjectIO OR 'Amber forcefield' OR 'amber forcefield'" \
  --state all
```

## Results Summary

### StackOverflowError

**Search Terms:** `StackOverflowError`, `stackoverflow`, `stack overflow`

**Results:** 
- **No issues found** (0 open, 0 closed)

**Conclusion:** The stack overflow problem during deep object copying does not appear to have been reported as a GitHub issue.

### Serialization Issues

**Search Terms:** `NotSerializableException`, `serialization`, `Serializable`, `deepCopy`

**Results:**
- **2 issues found** (both closed)

#### Issue #199: "hashcode error on writing the 7th ensemble structure file"
- **Status:** Closed (completed)
- **Opened:** June 6, 2025
- **Closed:** June 12, 2025
- **Link:** https://github.com/donaldlab/OSPREY3/issues/199

**Description:**
- Error: `java.lang.IllegalArgumentException: Key.hashCode() changed after serialization`
- Occurred when writing ensemble structure files using `--save-confs 4` option
- Happened after 6 successful file writes, on the 7th attempt
- Stack trace shows: `org.mapdb.HTreeMap.put()` → `edu.duke.cs.osprey.confspace.ConfDB.getSequence()`

**Root Cause (per maintainer):**
- Incorrect YAML configuration marking wildtype residue as mutable
- Sequence objects with wildtype incorrectly marked as mutant get serialized/deserialized incorrectly
- OSPREY converts uppercase mutant (TYR) to lowercase wildtype (tyr) after deserialization
- Results in mismatched hashCodes for MapDB keys

**Resolution:**
- Fix: Remove wildtype from list of allowed mutations in YAML configuration
- Different issue than ours (related to Sequence objects, not Amber parameter classes)

#### Issue #115: "Error in OSPREY3/examples/python.GMEC/findGMEC.cats.py"
- **Status:** Closed
- **Opened:** ~5 years ago
- **Link:** https://github.com/donaldlab/OSPREY3/issues/115

**Note:** Issue #115 mentions serialization but appears to be a different context/error.

**Conclusion:** The specific `NotSerializableException` errors we fixed (missing `Serializable` on Amber parameter classes) do not appear to have been reported separately.

### Amber Forcefield Files

**Search Terms:** `ForcefieldFileParser`, `AtomSymbolAndMass`, `ObjectIO`, `Amber forcefield`, `amber forcefield`

**Results:**
- **14 issues found**

**Issues Mentioned:**
- #160: Unknown atom types processed silently
- #131: Poisson Boltzmann Energy Function
- #140: Check how Cystines are handled in ResidueCache
- #164: How to interpret Kstar results
- #163: Residue(s) could not be matched to templates
- #188: Bug: SequenceAnalyzer doesn't work because confdb deleted
- #162: Do not use all CPU cores
- #185: Error when running prep.py
- #173: Forcefield file missing
- #179: TypeError with structure
- #98: Generating a custom template library
- #102: CHARMM forcefields can't be used
- #151: Using OSPREY for protein complex or drug design
- #26: Questions about epsilon value calculation

**Analysis:**
- None of these issues directly address missing `Serializable` implementations
- Most relate to:
  - Forcefield usage/configuration
  - Atom type matching
  - Template library generation
  - Different error types (TypeError, missing files, etc.)
- Issue #188 mentions `confdb` (configuration database) but is about deletion, not serialization

**Conclusion:** None of the 14 issues related to Amber forcefield files address the specific serialization problems we identified and fixed.

## Overall Conclusion

**The specific issues we fixed have NOT been reported as GitHub issues:**

1. **StackOverflowError during deep copying** - No matching issues found
2. **Missing `Serializable` on Amber parameter classes** - No matching issues found

**Related but different issues:**
- Issue #199 involves serialization but is a different problem (hashCode mismatch in MapDB with Sequence objects, caused by YAML configuration error)
- Other serialization/forcefield issues are unrelated to our specific fixes

**Implications:**
- These are likely previously unknown/unreported issues
- The fixes we implemented address real problems that prevent official examples from running
- The issues may have been silently affecting users who:
  - Never ran the official examples
  - Had different JVM stack sizes configured
  - Avoided code paths that trigger deep copying with Amber parameters

## References

- [OSPREY3 Issues Search](https://github.com/donaldlab/OSPREY3/issues)
- [Issue #199](https://github.com/donaldlab/OSPREY3/issues/199)
- [Issue #115](https://github.com/donaldlab/OSPREY3/issues/115)
- [Issue #188](https://github.com/donaldlab/OSPREY3/issues/188)

