# TEST2RL0ONLYONEMUTANT_ALLOCATION Profile Analysis

Total samples: 464

## Infrastructure vs Application Code

- Infrastructure (Gradle/JVM): 46,760 samples (10077.6%)
- Application code: -46,296 samples (-9977.6%)

## Top Application Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `all` | 464 | 100.00% |
| `byte[]` | 218 | 46.98% |
| `java/net/URLClassLoader$1.run` | 212 | 45.69% |
| `java/nio/file/Files.walkFileTree` | 156 | 33.62% |
| `java/security/AccessController.doPrivileged` | 128 | 27.59% |
| `java/security/AccessController.executePrivileged` | 128 | 27.59% |
| `java/net/URLClassLoader.findClass` | 121 | 26.08% |
| `java/lang/String.<init>` | 92 | 19.83% |
| `java/net/URLClassLoader.defineClass` | 84 | 18.10% |
| `java/util/Spliterators$IteratorSpliterator.forEachRemaining` | 69 | 14.87% |
| `java/util/Iterator.forEachRemaining` | 69 | 14.87% |
| `java/lang/Throwable.fillInStackTrace` | 66 | 14.22% |
| `com/esotericsoftware/kryo/io/Input.readString` | 57 | 12.28% |
| `org/jetbrains/kotlin/gradle/tasks/KotlinCompile.setupCompilerArgs` | 54 | 11.64% |
| `org/jetbrains/kotlin/gradle/internal/KotlinJvmCompilerArgumentsContributor.contr` | 54 | 11.64% |
| `java/lang/StringUTF16.compress` | 46 | 9.91% |
| `com/esotericsoftware/kryo/io/Input.<init>` | 44 | 9.48% |
| `java.lang.ThreadLocal$ThreadLocalMap$Entry` | 39 | 8.41% |
| `java/util/Arrays.copyOf` | 37 | 7.97% |
| `java/util/zip/ZipFile$ZipFileInflaterInputStream.<init>` | 34 | 7.33% |
| `java/lang/ClassNotFoundException.<init>` | 33 | 7.11% |
| `java/lang/Exception.<init>` | 33 | 7.11% |
| `java/lang/Throwable.<init>` | 33 | 7.11% |
| `com/sun/beans/finder/ClassFinder.findClass` | 31 | 6.68% |
| `java/security/SecureClassLoader.defineClass` | 29 | 6.25% |
| `com/google/common/base/Suppliers$NonSerializableMemoizingSupplier.get` | 28 | 6.03% |
| `org/jetbrains/kotlin/gradle/tasks/KotlinCompile_Decorated.getFilteredArgumentsMa` | 27 | 5.82% |
| `org/jetbrains/kotlin/gradle/tasks/AbstractKotlinCompileTool.getFilteredArguments` | 27 | 5.82% |
| `org/jetbrains/kotlin/gradle/internal/CompilerArgumentAwareWithInput$DefaultImpls` | 27 | 5.82% |
| `org/jetbrains/kotlin/gradle/internal/CompilerArgumentAware$DefaultImpls.getFilte` | 27 | 5.82% |

## OSPREY Application Hotspots

| Category | Samples | Percentage | Function Count |
|----------|---------|------------|----------------|
| native_energy | 14 | 3.02% | 7 functions |

### Detailed Function Lists

**native_energy** (3.02% of total):
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
