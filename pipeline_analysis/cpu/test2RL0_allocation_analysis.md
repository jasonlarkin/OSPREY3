# TEST2RL0_ALLOCATION Profile Analysis

Total samples: 1,741

## Infrastructure vs Application Code

- Infrastructure (Gradle/JVM): 282,108 samples (16203.8%)
- Application code: -280,367 samples (-16103.8%)

## Top Application Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `all` | 1,741 | 100.00% |
| `java/net/URLClassLoader$1.run` | 1,414 | 81.22% |
| `java/nio/file/Files.walkFileTree` | 1,178 | 67.66% |
| `java/security/AccessController.doPrivileged` | 887 | 50.95% |
| `java/security/AccessController.executePrivileged` | 887 | 50.95% |
| `java/net/URLClassLoader.findClass` | 782 | 44.92% |
| `byte[]` | 713 | 40.95% |
| `java/util/Spliterators$IteratorSpliterator.forEachRemaining` | 563 | 32.34% |
| `java/util/Iterator.forEachRemaining` | 563 | 32.34% |
| `java/net/URLClassLoader.defineClass` | 544 | 31.25% |
| `java/lang/Throwable.fillInStackTrace` | 388 | 22.29% |
| `java/util/Arrays.copyOf` | 336 | 19.30% |
| `java/beans/Introspector.getBeanInfo` | 249 | 14.30% |
| `java/util/zip/ZipFile$ZipEntryIterator.nextElement` | 214 | 12.29% |
| `com/sun/beans/finder/ClassFinder.findClass` | 209 | 12.00% |
| `java/lang/ClassNotFoundException.<init>` | 194 | 11.14% |
| `java/lang/Exception.<init>` | 194 | 11.14% |
| `java/lang/Throwable.<init>` | 194 | 11.14% |
| `SimpleTemplateScript1.run` | 185 | 10.63% |
| `java/security/SecureClassLoader.defineClass` | 160 | 9.19% |
| `java/util/zip/ZipFile$ZipFileInflaterInputStream.<init>` | 146 | 8.39% |
| `java/lang/Class.forName` | 140 | 8.04% |
| `java/lang/Class.forName0` | 140 | 8.04% |
| `java/util/Arrays.copyOfRange` | 110 | 6.32% |
| `java/util/zip/ZipFile.getZipEntry` | 108 | 6.20% |
| `java.lang.Object[]` | 107 | 6.15% |
| `java/util/zip/ZipFile$ZipEntryIterator.next` | 107 | 6.15% |
| `java/lang/String.<init>` | 94 | 5.40% |
| `long[]` | 87 | 5.00% |
| `java/lang/StringLatin1.newString` | 84 | 4.82% |

## OSPREY Application Hotspots

| Category | Samples | Percentage | Function Count |
|----------|---------|------------|----------------|
| native_energy | 75 | 4.31% | 11 functions |

### Detailed Function Lists

**native_energy** (4.31% of total):
- `java/lang/invoke/MethodHandleNatives.linkCallSite`
- `java/lang/invoke/MethodHandleNatives.linkCallSiteImpl`
- `java/lang/invoke/MethodHandleNatives.linkMethod`
- `java/lang/invoke/MethodHandleNatives.linkMethodHandleConstant`
- `java/lang/invoke/MethodHandleNatives.linkMethodImpl`
- `java/lang/invoke/MethodHandleNatives.varHandleOperationLinkerMethod`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher$1.run`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher.access$100`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher.executeRunLoop0`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcherCallback.reportTermination`
- ... (1 more)

## Workload Characteristics

### CPU Processing Patterns

- Native energy calculations (C++) are CPU-intensive
