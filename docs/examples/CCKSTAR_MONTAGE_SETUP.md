# MONTAGE Setup and Database Configuration

## Current Status

The MASTER database configuration in `resources/db.txt` points to paths from a different system:
- Path: `/home/users/hc340/dlab/henry/master-db/`
- Status: **Not accessible** on current system
- Total paths configured: 119,140 database files

## MASTER Database Setup

### Option 1: Automated Download (Recommended)

Use the provided script to download and set up the MASTER database:

```bash
cd src/main/python/CCKStar
./download_master_db.sh
```

The script will:
1. Check if `rsync` is installed (required for download)
2. Download the database using rsync from `grigoryanlab.org::masterDB/`
3. Update the `list` file with correct local paths
4. Create the `resources/db.txt.local` config file

**Prerequisites:**
- `rsync` must be installed:
  - Ubuntu/Debian: `sudo apt-get install rsync`
  - macOS: Pre-installed
  - Windows: Use WSL or Linux environment
- Sufficient disk space (10+ GB recommended)
- Download time: 30 minutes to several hours depending on connection speed

### Option 2: Manual rsync Download

If you prefer to download manually:

```bash
cd src/main/python/CCKStar
mkdir -p master-db
rsync -varz --progress grigoryanlab.org::masterDB/ master-db/

# Update the list file paths (replace /local/path with actual path)
sed -i "s|/local/path|$(pwd)/master-db|g" master-db/list

# Create config file
python3 check_master_db.py --create-config master-db/ --output resources/db.txt.local
```

### Option 3: Alternative Download Methods

If rsync is not available:
1. **Visit MASTER website**: https://grigoryanlab.org/master/
2. **Download database files**: 
   - Database is large (several GB)
   - Files are in `.pds` format (Protein Data Structure)
3. **Extract to local directory**:
   ```bash
   mkdir ~/master-db
   # Extract downloaded files to ~/master-db/
   ```
4. **Create local config**:
   ```bash
   cd src/main/python/CCKStar
   python3 check_master_db.py --create-config ~/master-db/ --output resources/db.txt.local
   ```
5. **Update MONTAGE to use local config**:
   - Modify `MONTAGE.py` to use `db.txt.local` instead of `db.txt`
   - Or replace `db.txt` with local paths

### Option 2: Use Minimal Test Database

For testing purposes, you can create a minimal database with a few structures:

```bash
# Create minimal test database
mkdir -p test-master-db
# Copy a few .pds files (if available) or create test files
python3 check_master_db.py --create-config test-master-db/ --output resources/db.txt.test
```

### Option 3: Skip MASTER (Test Workflow Only)

For workflow demonstration without MASTER:
- Use the `test_montage_demo.py` script (already created)
- Demonstrates all steps except actual MASTER search
- Shows SCOPE integration and flexibility assignment

## Database Configuration

### Current Config Format

`resources/db.txt` contains one path per line:
```
/home/users/hc340/dlab/henry/master-db/101M.pds
/home/users/hc340/dlab/henry/master-db/103L.pds
...
```

### Creating Local Config

Use the `check_master_db.py` script:

```bash
# Check current config
python3 check_master_db.py --check

# Search for databases
python3 check_master_db.py --search

# Create config from directory
python3 check_master_db.py --create-config /path/to/master-db/ --output resources/db.txt.local
```

## MASTER Executable

The MASTER executable is present:
- Location: `resources/master`
- Status: ✓ Available
- Usage: `./resources/master --query query.pds --targetList db.txt --topN 20`

## Testing Without Full Database

### Workflow Demonstration

The `test_montage_demo.py` script demonstrates:
1. ✓ Scaffold preparation (chain renaming, extraction, reflection)
2. ✓ SCOPE flexibility assignment
3. ✗ MASTER search (requires database)
4. ✗ Scaffold generation (requires MASTER output)
5. ✗ K* evaluation (requires scaffolds)

### Partial Execution

You can test individual components:

```python
# Test scaffold preparation
from MONTAGE import complex_renamer, extract_design, reflect_design
renamed = complex_renamer('input.pdb', 'A', 'z')
design = extract_design(renamed, 'A', 'z')

# Test SCOPE flexibility
from MONTAGE import target_flex_MONTAGE
flex_residues = target_flex_MONTAGE('input.pdb', 'L', 4)
```

## Alternative: Use Pre-computed Scaffolds

If you have access to pre-computed scaffolds from a previous MONTAGE run:
1. Skip MASTER search step
2. Use existing scaffolds from `*-MASTER/scaffolds/` directory
3. Proceed directly to K* evaluation

## Documentation

- Database check script: `src/main/python/CCKStar/check_master_db.py`
- MONTAGE demo: `src/main/python/CCKStar/test_montage_demo.py`
- MASTER website: https://grigoryanlab.org/master/
- MONTAGE analysis: `docs/examples/CCKSTAR_MONTAGE_ANALYSIS.md`

## Next Steps

1. **If you have MASTER database access**:
   - Download/obtain database files
   - Run `check_master_db.py --create-config` to set up paths
   - Run full MONTAGE workflow

2. **If you don't have database access**:
   - Use `test_montage_demo.py` for workflow demonstration
   - Contact Grigoryan Lab for database access
   - Or use pre-computed scaffolds if available

3. **For DSL compiler integration**:
   - Document MASTER database as a prerequisite
   - Provide setup instructions in DSL compiler docs
   - Consider making database path configurable

