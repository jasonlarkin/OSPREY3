# TEST2RL0ONLYONEMUTANT_CPU Profile Analysis

Total samples: 363

## Infrastructure vs Application Code

- Infrastructure (Gradle/JVM): 15,303 samples (4215.7%)
- Application code: -14,940 samples (-4115.7%)

## Top Application Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `all` | 363 | 100.00% |
| `java/net/URLClassLoader$1.run` | 82 | 22.59% |
| `ParseGenerator::generate(JVMState*)` | 57 | 15.70% |
| `Parse::Parse(JVMState*, ciMethod*, float)` | 57 | 15.70% |
| `Parse::do_all_blocks()` | 55 | 15.15% |
| `Parse::do_one_block()` | 55 | 15.15% |
| `Parse::do_call()` | 49 | 13.50% |
| `java/net/URLClassLoader.findClass` | 42 | 11.57% |
| `java/security/AccessController.doPrivileged` | 42 | 11.57% |
| `java/security/AccessController.executePrivileged` | 42 | 11.57% |
| `PhaseChaitin::Register_Allocate()` | 39 | 10.74% |
| `java/net/URLClassLoader.defineClass` | 35 | 9.64% |
| `PhaseIdealLoop::optimize(PhaseIterGVN&, LoopOptsMode)` | 31 | 8.54% |
| `java/security/SecureClassLoader.defineClass` | 28 | 7.71% |
| `PhaseIdealLoop::build_and_optimize()` | 26 | 7.16% |
| `Java_java_lang_ClassLoader_defineClass1` | 24 | 6.61% |
| `JVM_DefineClassWithSource` | 24 | 6.61% |
| `PredictedCallGenerator::generate(JVMState*)` | 23 | 6.34% |
| `java/util/ArrayList$ArrayListSpliterator.forEachRemaining` | 21 | 5.79% |
| `Compiler::compile_method(ciEnv*, ciMethod*, int, bool, DirectiveSet*)` | 18 | 4.96% |
| `Compilation::Compilation(AbstractCompiler*, ciEnv*, ciMethod*, int, BufferBlob*,` | 18 | 4.96% |
| `Compilation::compile_method()` | 18 | 4.96% |
| `Compilation::compile_java_method()` | 17 | 4.68% |
| `org/jetbrains/kotlin/gradle/plugin/statistics/KotlinBuildStatHandler$buildFinish` | 16 | 4.41% |
| `jdk/proxy3/$Proxy103.completed` | 15 | 4.13% |
| `G1DirtyCardQueueSet::refine_completed_buffer_concurrently(unsigned int, unsigned` | 15 | 4.13% |
| `G1DirtyCardQueueSet::refine_buffer(BufferNode*, unsigned int, G1ConcurrentRefine` | 15 | 4.13% |
| `PhaseChaitin::build_ifg_physical(ResourceArea*)` | 15 | 4.13% |
| `GraphBuilder::iterate_all_blocks(bool)` | 15 | 4.13% |
| `GraphBuilder::iterate_bytecodes_for_block(int)` | 15 | 4.13% |

## OSPREY Application Hotspots

| Category | Samples | Percentage | Function Count |
|----------|---------|------------|----------------|
| native_energy | 20 | 5.51% | 17 functions |

### Detailed Function Lists

**native_energy** (5.51% of total):
- `JNIEnv_::NewString(unsigned short const*, int)`
- `Java_net_rubygrapefruit_platform_internal_jni_PosixFileSystemFunctions_listFileSystems`
- `java/lang/invoke/MethodHandleNatives.isCallerSensitive`
- `java/lang/invoke/MethodHandleNatives.linkCallSite`
- `java/lang/invoke/MethodHandleNatives.linkCallSiteImpl`
- `java/lang/invoke/MethodHandleNatives.linkMethodHandleConstant`
- `java/lang/invoke/MethodHandleNatives.resolve`
- `jni_GetArrayLength`
- `jni_GetByteArrayRegion`
- `jni_NewString`
- ... (7 more)

## Workload Characteristics

### CPU Processing Patterns

- Native energy calculations (C++) are CPU-intensive
