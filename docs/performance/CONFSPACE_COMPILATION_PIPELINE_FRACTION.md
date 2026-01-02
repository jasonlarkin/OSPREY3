# ConfSpace Compilation Fraction of Overall Pipeline

## Pipeline Time Breakdown (Medium System: 10K pairs, 100 sequences)

### Stage Times

| Stage | Time | Notes |
|-------|------|-------|
| **K* Execution** | 86,400 - 172,800s (1-2 days) | **DOMINANT** - 96-98% of total |
| **MONTAGE** | 2,191s (36.5 min) | 1.3-2.5% of total |
| **SCOPE** | 208s (3.5 min) | 0.1-0.2% of total |
| **ARISE** | ~23s | Blocked by K*, negligible |
| **Total** | ~86,400 - 172,800s | |

### MONTAGE Breakdown

| Component | Time | % of MONTAGE | % of Total Pipeline |
|-----------|------|--------------|---------------------|
| **ConfSpace Compilation** | 1,744s (29 min) | **79.6%** | **1.0-2.0%** |
| SCOPE operations | 188s (3.1 min) | 8.6% | 0.1-0.2% |
| MASTER subprocess | 63s (1.1 min) | 2.9% | <0.1% |
| Other (I/O, JVM startup) | 196s (3.3 min) | 9.0% | 0.1-0.2% |
| **Total MONTAGE** | 2,191s (36.5 min) | 100% | 1.3-2.5% |

## ConfSpace Compilation as Fraction of Total Pipeline

### Per Match
- **Minimum (1 day K*):** 1,744s / 86,400s = **2.0%**
- **Maximum (2 day K*):** 1,744s / 172,800s = **1.0%**

### Per MONTAGE Run (4 matches observed)
- **Total ConfSpace:** ~1,744s × 4 = ~6,976s (116 min = 1.9 hours)
- **Minimum (1 day K*):** 6,976s / 86,400s = **8.1%**
- **Maximum (2 day K*):** 6,976s / 172,800s = **4.0%**

## Key Insights

### 1. K* Dominates Pipeline
- **96-98% of total time** is K* execution
- ConfSpace compilation is **1-2% per match**, **4-8% per MONTAGE run**
- Even 50x speedup on ConfSpace = only 2-4% total pipeline speedup

### 2. MONTAGE is Small
- **1.3-2.5% of total pipeline time**
- ConfSpace compilation = **80% of MONTAGE**, but MONTAGE itself is small
- Optimizing MONTAGE helps, but K* is the real bottleneck

### 3. Optimization Impact

**ConfSpace Compilation Optimizations:**
- **10x speedup:** 1,744s → 174s per match
  - Saves: 1,570s (26 min) per match
  - Pipeline impact: 0.9-1.8% total speedup
- **50x speedup:** 1,744s → 35s per match
  - Saves: 1,709s (28 min) per match
  - Pipeline impact: 1.0-2.0% total speedup

**K* Optimizations:**
- **10x speedup:** 86,400s → 8,640s (2.4 hours)
  - Saves: 77,760s (21.6 hours)
  - Pipeline impact: **90% total speedup**

## Conclusion

**ConfSpace compilation (LEaP) is:**
- **1-2% of total pipeline time** (per match)
- **4-8% of total pipeline time** (per MONTAGE run with 4 matches)
- **80% of MONTAGE time** (but MONTAGE is only 1.3-2.5% of total)

**Optimization Priority:**
1. **K* parallelization** - 10-100x speedup, **90% pipeline impact** (P0)
2. **ConfSpace compilation** - 10-50x speedup, **1-2% pipeline impact** (P2)
3. **SCOPE optimization** - 20-400x speedup, **0.1-0.2% pipeline impact** (P3)

**Recommendation:**
- ConfSpace compilation optimizations (batching, caching) are **worth doing** but **not critical**
- Focus on K* parallelization for maximum impact
- ConfSpace optimizations can be done in parallel with AWS/SLURM setup
- With larger resources, batching/caching will be easier to test and validate

## When ConfSpace Optimization Matters

**High-volume scenarios:**
- Many matches per design (10+ matches)
- Multiple designs in parallel
- Iterative design workflows (many MONTAGE runs)

**In these cases:**
- ConfSpace compilation can become 10-20% of total time
- Optimizations become more valuable
- Still secondary to K* optimization

