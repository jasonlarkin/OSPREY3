# TEST2RL0_CPU Profile Analysis

Total samples: 900

## Infrastructure vs Application Code

- Infrastructure (Gradle/JVM): 49,295 samples (5477.2%)
- Application code: -48,395 samples (-5377.2%)

## Top Application Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `all` | 900 | 100.00% |
| `PhaseChaitin::Register_Allocate()` | 194 | 21.56% |
| `ParseGenerator::generate(JVMState*)` | 155 | 17.22% |
| `Parse::Parse(JVMState*, ciMethod*, float)` | 155 | 17.22% |
| `Parse::do_all_blocks()` | 152 | 16.89% |
| `Parse::do_one_block()` | 151 | 16.78% |
| `Parse::do_call()` | 134 | 14.89% |
| `PredictedCallGenerator::generate(JVMState*)` | 111 | 12.33% |
| `java/net/URLClassLoader$1.run` | 108 | 12.00% |
| `PhaseIdealLoop::optimize(PhaseIterGVN&, LoopOptsMode)` | 92 | 10.22% |
| `PhaseIdealLoop::build_and_optimize()` | 83 | 9.22% |
| `java/security/AccessController.doPrivileged` | 63 | 7.00% |
| `java/security/AccessController.executePrivileged` | 62 | 6.89% |
| `java/nio/file/Files.walkFileTree` | 62 | 6.89% |
| `java/net/URLClassLoader.findClass` | 57 | 6.33% |
| `Compiler::compile_method(ciEnv*, ciMethod*, int, bool, DirectiveSet*)` | 50 | 5.56% |
| `Compilation::Compilation(AbstractCompiler*, ciEnv*, ciMethod*, int, BufferBlob*,` | 50 | 5.56% |
| `Compilation::compile_method()` | 50 | 5.56% |
| `java/net/URLClassLoader.defineClass` | 49 | 5.44% |
| `java/security/SecureClassLoader.defineClass` | 46 | 5.11% |
| `PhaseIterGVN::optimize()` | 38 | 4.22% |
| `Java_java_lang_ClassLoader_defineClass1` | 36 | 4.00% |
| `PhaseIterGVN::transform_old(Node*)` | 36 | 4.00% |
| `JVM_DefineClassWithSource` | 35 | 3.89% |
| `Compilation::compile_java_method()` | 35 | 3.89% |
| `PhaseChaitin::build_ifg_physical(ResourceArea*)` | 33 | 3.67% |
| `SymbolTable::do_lookup(char const*, int, unsigned long)` | 32 | 3.56% |
| `PhaseLive::compute(unsigned int)` | 31 | 3.44% |
| `IdealLoopTree::loop_predication(PhaseIdealLoop*)` | 30 | 3.33% |
| `PhaseIdealLoop::build_loop_late(VectorSet&, Node_List&, Node_Stack&)` | 26 | 2.89% |

## OSPREY Application Hotspots

| Category | Samples | Percentage | Function Count |
|----------|---------|------------|----------------|
| native_energy | 11 | 1.22% | 7 functions |

### Detailed Function Lists

**native_energy** (1.22% of total):
- `java/lang/invoke/MethodHandleNatives.findMethodHandleType`
- `java/lang/invoke/MethodHandleNatives.linkCallSite`
- `java/lang/invoke/MethodHandleNatives.linkCallSiteImpl`
- `java/lang/invoke/MethodHandleNatives.linkMethodHandleConstant`
- `jni_ExceptionOccurred`
- `jni_GetObjectField`
- `sun/nio/fs/UnixNativeDispatcher.readdir`

## Workload Characteristics

### CPU Processing Patterns

- Native energy calculations (C++) are CPU-intensive
