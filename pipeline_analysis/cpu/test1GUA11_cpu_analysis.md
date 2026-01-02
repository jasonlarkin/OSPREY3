# TEST1GUA11_CPU Profile Analysis

Total samples: 771

## Infrastructure vs Application Code

- Infrastructure (Gradle/JVM): 30,508 samples (3956.9%)
- Application code: -29,737 samples (-3856.9%)

## Top Application Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `all` | 771 | 100.00% |
| `PhaseChaitin::Register_Allocate()` | 145 | 18.81% |
| `ParseGenerator::generate(JVMState*)` | 128 | 16.60% |
| `Parse::Parse(JVMState*, ciMethod*, float)` | 128 | 16.60% |
| `Parse::do_all_blocks()` | 123 | 15.95% |
| `Parse::do_one_block()` | 119 | 15.43% |
| `Parse::do_call()` | 103 | 13.36% |
| `java/net/URLClassLoader$1.run` | 96 | 12.45% |
| `PhaseIdealLoop::optimize(PhaseIterGVN&, LoopOptsMode)` | 83 | 10.77% |
| `PredictedCallGenerator::generate(JVMState*)` | 80 | 10.38% |
| `PhaseIdealLoop::build_and_optimize()` | 76 | 9.86% |
| `java/security/AccessController.doPrivileged` | 57 | 7.39% |
| `java/security/AccessController.executePrivileged` | 57 | 7.39% |
| `java/net/URLClassLoader.findClass` | 49 | 6.36% |
| `java/net/URLClassLoader.defineClass` | 37 | 4.80% |
| `java/security/SecureClassLoader.defineClass` | 34 | 4.41% |
| `ConnectionGraph::find_inst_mem(Node*, int, GrowableArray<PhiNode*>&)` | 33 | 4.28% |
| `Compiler::compile_method(ciEnv*, ciMethod*, int, bool, DirectiveSet*)` | 33 | 4.28% |
| `Compilation::Compilation(AbstractCompiler*, ciEnv*, ciMethod*, int, BufferBlob*,` | 33 | 4.28% |
| `Compilation::compile_method()` | 33 | 4.28% |
| `PhaseIterGVN::optimize()` | 32 | 4.15% |
| `Java_java_lang_ClassLoader_defineClass1` | 30 | 3.89% |
| `JVM_DefineClassWithSource` | 30 | 3.89% |
| `PhaseIterGVN::transform_old(Node*)` | 29 | 3.76% |
| `PhaseChaitin::build_ifg_physical(ResourceArea*)` | 28 | 3.63% |
| `PhaseLive::compute(unsigned int)` | 27 | 3.50% |
| `ConnectionGraph::split_memory_phi(PhiNode*, int, GrowableArray<PhiNode*>&)` | 27 | 3.50% |
| `Matcher::match()` | 24 | 3.11% |
| `SymbolTable::do_lookup(char const*, int, unsigned long)` | 23 | 2.98% |
| `Compilation::compile_java_method()` | 22 | 2.85% |

## OSPREY Application Hotspots

| Category | Samples | Percentage | Function Count |
|----------|---------|------------|----------------|
| native_energy | 16 | 2.08% | 12 functions |

### Detailed Function Lists

**native_energy** (2.08% of total):
- `JNIHandleBlock::allocate_handle(oopDesc*, AllocFailStrategy::AllocFailEnum)`
- `NativeCall::destination() const`
- `VM::loadMethodIDs(_jvmtiEnv*, JNIEnv_*, _jclass*)`
- `java/lang/invoke/MethodHandleNatives.linkCallSite`
- `java/lang/invoke/MethodHandleNatives.linkCallSiteImpl`
- `jni_GetObjectField`
- `jni_GetStringUTFRegion`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher.shutdown`
- `net/rubygrapefruit/platform/internal/jni/AbstractFileEventFunctions$NativeFileWatcher.shutdown0`
- `net/rubygrapefruit/platform/internal/jni/LinuxFileEventFunctions$LinuxFileWatcher.shutdown`
- ... (2 more)

## Workload Characteristics

### CPU Processing Patterns

- Native energy calculations (C++) are CPU-intensive
