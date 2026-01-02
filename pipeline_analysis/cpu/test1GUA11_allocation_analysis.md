# TEST1GUA11_ALLOCATION Profile Analysis

Total samples: 626

## Infrastructure vs Application Code

- Infrastructure (Gradle/JVM): 74,018 samples (11824.0%)
- Application code: -73,392 samples (-11724.0%)

## Top Application Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `all` | 626 | 100.00% |
| `byte[]` | 229 | 36.58% |
| `java/net/URLClassLoader$1.run` | 130 | 20.77% |
| `java/lang/String.<init>` | 90 | 14.38% |
| `com/esotericsoftware/kryo/io/Input.<init>` | 90 | 14.38% |
| `java/nio/file/Files.walkFileTree` | 89 | 14.22% |
| `java/security/AccessController.doPrivileged` | 77 | 12.30% |
| `java/security/AccessController.executePrivileged` | 77 | 12.30% |
| `java/net/URLClassLoader.findClass` | 76 | 12.14% |
| `java/util/Arrays.copyOf` | 57 | 9.11% |
| `java/net/URLClassLoader.defineClass` | 55 | 8.79% |
| `com/esotericsoftware/kryo/io/Input.readString` | 52 | 8.31% |
| `java/lang/StringUTF16.compress` | 44 | 7.03% |
| `java/util/Spliterators$IteratorSpliterator.forEachRemaining` | 44 | 7.03% |
| `java/util/Iterator.forEachRemaining` | 44 | 7.03% |
| `java.lang.Object[]` | 42 | 6.71% |
| `java/lang/Throwable.fillInStackTrace` | 42 | 6.71% |
| `org/jetbrains/kotlin/gradle/tasks/KotlinCompile.setupCompilerArgs` | 30 | 4.79% |
| `org/jetbrains/kotlin/gradle/internal/KotlinJvmCompilerArgumentsContributor.contr` | 30 | 4.79% |
| `java/util/HashMap.computeIfAbsent` | 25 | 3.99% |
| `java/security/SecureClassLoader.defineClass` | 23 | 3.67% |
| `java.util.concurrent.locks.AbstractQueuedSynchronizer$ConditionNode` | 22 | 3.51% |
| `java.lang.String` | 22 | 3.51% |
| `java/lang/ClassNotFoundException.<init>` | 21 | 3.35% |
| `java/lang/Exception.<init>` | 21 | 3.35% |
| `java/lang/Throwable.<init>` | 21 | 3.35% |
| `java/beans/Introspector.getBeanInfo` | 21 | 3.35% |
| `java/util/ArrayList.add` | 18 | 2.88% |
| `java/util/ArrayList.grow` | 18 | 2.88% |
| `java/lang/StringLatin1.newString` | 18 | 2.88% |

## OSPREY Application Hotspots

| Category | Samples | Percentage | Function Count |
|----------|---------|------------|----------------|
| native_energy | 11 | 1.76% | 7 functions |

### Detailed Function Lists

**native_energy** (1.76% of total):
- `java/lang/invoke/MethodHandleNatives.linkCallSite`
- `java/lang/invoke/MethodHandleNatives.linkCallSiteImpl`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher$1.run`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher.access$100`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher.executeRunLoop0`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcherCallback.reportTermination`
- `sun/nio/fs/UnixNativeDispatcher.readdir`

## Workload Characteristics

### CPU Processing Patterns

- Native energy calculations (C++) are CPU-intensive
