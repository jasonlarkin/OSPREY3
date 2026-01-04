# MASTER Database Setup Status

## Current Status: ✅ COMPLETE

The MASTER database has been successfully downloaded and configured for use with MONTAGE.

## Database Details

- **Location**: `src/main/python/CCKStar/master-db/`
- **Total Files**: 14,546 .pds files
- **Total Size**: ~4.4 GB
- **Download Method**: rsync from `grigoryanlab.org::masterDB/`
- **Config File**: `resources/db.txt.local` (14,546 paths)

## Database Structure

The database is organized in subdirectories by PDB ID prefix:
```
master-db/
├── 55/
│   └── 155c_A.pds
├── 6v/
│   └── 16vp_A.pds
├── 91/
│   └── 1914_A.pds
├── a0/
│   ├── 1a0b_A.pds
│   └── 1a0g_A.pds
├── ...
└── list (MASTER list file)
```

## Configuration Files

### Local Config (`resources/db.txt.local`)
- Contains 14,546 absolute paths to .pds files
- All paths verified and accessible
- Ready for use with MONTAGE

### Original Config (`resources/db.txt`)
- Contains 119,140 paths from original system
- Paths point to `/home/users/hc340/dlab/henry/master-db/`
- Not accessible on current system
- Kept for reference

## Using the Database

### Option 1: Update MONTAGE.py

Modify `MONTAGE.py` to use the local config:

```python
# Change:
MASTER_database = "./resources/db.txt"
# To:
MASTER_database = "./resources/db.txt.local"
```

### Option 2: Use MASTER list file

The downloaded database includes a `list` file at `master-db/list` that has been updated with correct paths. You can use this directly:

```python
MASTER_database = "./master-db/list"
```

### Option 3: Create symlink

```bash
cd src/main/python/CCKStar/resources
mv db.txt db.txt.original
ln -s db.txt.local db.txt
```

## Verification

To verify the database setup:

```bash
cd src/main/python/CCKStar
python3 check_master_db.py --check
```

Or verify specific files:

```bash
# Count .pds files
find master-db -name "*.pds" | wc -l
# Should show: 14546

# Check config file
wc -l resources/db.txt.local
# Should show: 14546 resources/db.txt.local
```

## Download Information

- **Download Date**: December 21, 2024
- **Download Time**: ~30 minutes to several hours (depending on connection)
- **Source**: grigoryanlab.org::masterDB/
- **Database Version**: MASTER v1.6 compatible
- **Last Updated**: October 22, 2014 (as per MASTER website)

## Next Steps

1. ✅ Database downloaded
2. ✅ Config file created
3. ⏭️ Update MONTAGE.py to use local config
4. ⏭️ Run full MONTAGE workflow with MASTER search
5. ⏭️ Test scaffold generation and GMEC extraction

## Maintenance

### Re-downloading Database

If you need to re-download the database:

```bash
cd src/main/python/CCKStar
./download_master_db.sh
```

The script will:
- Check for existing files
- Prompt for confirmation
- Download using rsync
- Update paths automatically
- Create config file

### Updating Config

To regenerate the config file:

```bash
cd src/main/python/CCKStar
python3 check_master_db.py --create-config master-db/ --output resources/db.txt.local
```

## Notes

- The database is a single-chain database (multi-chain PDB entries are split)
- Database was built from PDB as of 10/22/2014
- Files are organized by PDB ID prefix for efficient access
- The `list` file in `master-db/` has been updated with correct local paths

