# Fast Iteration Tips for Java Development

## Problem
Gradle test runs take 3-5 minutes due to compilation, which is too slow for debugging.

## Solutions

### 1. Use Gradle Daemon (Already Enabled)
The Gradle daemon stays running between builds, avoiding startup overhead:
```bash
# First run: starts daemon (slower)
./gradlew test --tests "..."

# Subsequent runs: reuses daemon (much faster)
./gradlew test --tests "..."
```

### 2. Compile Only (Skip Tests)
When you just want to check if code compiles:
```bash
# Compile main code only
./gradlew compileJava

# Compile test code only
./gradlew compileTestJava

# Compile both
./gradlew compileJava compileTestJava
```

### 3. Run Single Test Class
Instead of running all tests, run just one class:
```bash
# Run specific test class
./gradlew test --tests "edu.duke.cs.osprey.tools.Trace1CC8Execution"

# Run specific test method
./gradlew test --tests "edu.duke.cs.osprey.tools.Trace1CC8Execution.trace1CC8Capture"
```

### 4. Use `--no-rebuild` When Possible
If you know dependencies haven't changed:
```bash
./gradlew test --tests "..." --no-rebuild
```

### 5. Skip Unnecessary Tasks
Skip tasks you don't need:
```bash
# Skip checkstyle, etc.
./gradlew test --tests "..." -x check -x javadoc
```

### 6. Use Standalone Java for Quick Tests
For very fast iteration, compile and run directly:
```bash
# Compile (one time)
./gradlew compileJava compileTestJava

# Run directly (much faster)
java -cp "build/classes/java/main:build/classes/java/test:$(find ~/.gradle -name "*.jar" | tr '\n' ':')" \
  org.junit.platform.console.ConsoleLauncher \
  --class-path build/classes/java/test \
  --select-class edu.duke.cs.osprey.tools.Trace1CC8Execution
```

### 7. Use `--rerun-tasks` Sparingly
Only use when you suspect caching issues:
```bash
# Normal (uses cache)
./gradlew test --tests "..."

# Force rerun (slower, only when needed)
./gradlew test --tests "..." --rerun-tasks
```

### 8. Fastest Workflow for Debugging

**For quick syntax/compilation checks:**
```bash
./gradlew compileJava compileTestJava
```

**For running tests (after compilation):**
```bash
./gradlew test --tests "..." --no-rebuild
```

**For full clean build (when needed):**
```bash
./gradlew clean test --tests "..."
```

## Expected Times

- **First run (cold):** 3-5 minutes (compilation + test execution)
- **Subsequent runs (warm):** 10-30 seconds (with daemon, incremental compilation)
- **Compile only:** 5-10 seconds
- **Standalone Java:** < 1 second (after compilation)

## Performance Settings

See `gradle.properties` for:
- Parallel execution
- Build cache
- Configuration on demand
- Increased daemon memory

## Tips

1. **Keep daemon running** - Don't kill it between builds
2. **Use incremental compilation** - Gradle automatically detects changed files
3. **Compile before running tests** - Use `compileJava` first to catch errors faster
4. **Use `--no-rebuild`** - When you know dependencies haven't changed
5. **Run single tests** - Don't run the whole suite during debugging

