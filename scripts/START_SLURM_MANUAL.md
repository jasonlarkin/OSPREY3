# Manual SLURM Start (For WSL Password Issues)

If the automated script has sudo password prompt issues, start SLURM manually:

## Step 1: Start Controller (Terminal 1)

```bash
sudo slurmctld -D
```

Keep this terminal open. It will show controller logs.

## Step 2: Start Daemon (Terminal 2)

Open a new terminal and run:

```bash
sudo slurmd -D
```

Keep this terminal open. It will show daemon logs.

## Step 3: Verify (Terminal 3)

Open a third terminal and verify:

```bash
sinfo
srun hostname
sbatch --wrap='echo "SLURM working"'
```

## Stopping SLURM

In any terminal:

```bash
sudo pkill slurmctld
sudo pkill slurmd
```

## Alternative: Background Processes

If you want to run in background:

```bash
# Terminal 1
sudo slurmctld -D > /tmp/slurmctld.log 2>&1 &

# Terminal 2  
sudo slurmd -D > /tmp/slurmd.log 2>&1 &
```

Check logs if issues:
```bash
tail -f /tmp/slurmctld.log
tail -f /tmp/slurmd.log
```

