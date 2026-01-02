# CPU_PROFILE_CPU Profile Analysis

Total samples: 363

## Top Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `org/gradle/internal/operations/DefaultBuildOperationRunner.execute` | 388 | 106.89% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner$2.execute` | 388 | 106.89% |
| `all` | 363 | 100.00% |
| `org/gradle/internal/concurrent/CompositeStoppable.stop` | 293 | 80.72% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner$CallableBuildOperatio` | 282 | 77.69% |
| `org/gradle/launcher/daemon/server/api/DaemonCommandExecution.proceed` | 264 | 72.73% |
| `start_thread` | 172 | 47.38% |
| `thread_native_entry(Thread*)` | 172 | 47.38% |
| `Thread::call_run()` | 172 | 47.38% |
| `JavaThread::thread_main_inner()` | 155 | 42.70% |
| `CompileBroker::compiler_thread_loop()` | 153 | 42.15% |
| `CompileBroker::invoke_compiler_on_method(CompileTask*)` | 153 | 42.15% |
| `org/gradle/internal/dispatch/ReflectionDispatch.dispatch` | 152 | 41.87% |
| `org/gradle/internal/concurrent/ExecutorPolicy$CatchAndRecordFailures.onExecute` | 150 | 41.32% |
| `org/gradle/internal/service/DefaultServiceRegistry$ManagedObjectServiceProvider.` | 150 | 41.32% |
| `org/gradle/internal/operations/DefaultBuildOperationExecutor.call` | 142 | 39.12% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner.call` | 142 | 39.12% |
| `java/lang/Thread.run` | 138 | 38.02% |
| `org/gradle/internal/concurrent/ThreadFactoryImpl$ManagedThreadRunnable.run` | 138 | 38.02% |
| `java/util/concurrent/ThreadPoolExecutor$Worker.run` | 138 | 38.02% |
| `java/util/concurrent/ThreadPoolExecutor.runWorker` | 138 | 38.02% |
| `org/gradle/internal/concurrent/ManagedExecutorImpl$1.run` | 135 | 37.19% |
| `java/lang/reflect/Method.invoke` | 135 | 37.19% |
| `jdk/internal/reflect/DelegatingMethodAccessorImpl.invoke` | 135 | 37.19% |
| `C2Compiler::compile_method(ciEnv*, ciMethod*, int, bool, DirectiveSet*)` | 131 | 36.09% |
| `Compile::Compile(ciEnv*, ciMethod*, int, bool, bool, bool, bool, bool, Directive` | 131 | 36.09% |
| `org/gradle/internal/event/AbstractBroadcastDispatch.dispatch` | 124 | 34.16% |
| `jdk/internal/reflect/NativeMethodAccessorImpl.invoke` | 116 | 31.96% |
| `jdk/internal/reflect/NativeMethodAccessorImpl.invoke0` | 115 | 31.68% |
| `java/util/Optional.orElseGet` | 104 | 28.65% |

## OSPREY-Specific Hotspots

| Category | Samples | Percentage | Functions |
|----------|---------|------------|-----------|
| allocation | 1,050 | 289.26% | java/util/concurrent/FutureTask.runAndReset, java/util/HashMap$ValueSpliterator.tryAdvance, java/util/HashMap$HashMapSpliterator.getFence, java/util/HashSet.iterator, java/util/HashMap$KeySet.iterator ... (198 total) |
| gc | 485 | 133.61% | org/gradle/launcher/daemon/server/health/gc/GarbageCollectionCheck.run, org/gradle/internal/execution/history/impl/FileCollectionFingerprintSerializer.read, org/gradle/internal/execution/history/impl/FileCollectionFingerprintSerializer.write, org/gradle/internal/execution/history/impl/FileCollectionFingerprintSerializer.writeRootHashes, com/google/common/collect/ImmutableMultimap.entries ... (70 total) |
| kstar | 183 | 50.41% | CodeHeap::next_used(HeapBlock*) const, CompileBroker::invoke_compiler_on_method(CompileTask*), PhaseCFG::hoist_to_cheaper_block(Block*, Block*, Node*), PhaseCFG::insert_anti_dependences(Block*, Node*, bool), PhaseCFG::schedule_local(Block*, GrowableArray<int>&, VectorSet&, long*) ... (12 total) |
| conformation | 157 | 43.25% | org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection.visitChildren, org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection$UnresolvedItemsCollector.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration$ConfigurationFileCollection.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration$ConfigurationFileCollection.getSelectedArtifacts ... (42 total) |
| astar | 64 | 17.63% | Deoptimization::query_update_method_data(MethodData*, int, Deoptimization::DeoptReason, bool, bool, Method*, unsigned int&, bool&, bool&), KlassFactory::create_from_stream(ClassFileStream*, Symbol*, ClassLoaderData*, ClassLoadInfo const&, JavaThread*), ClassFileParser::ClassFileParser(ClassFileStream*, Symbol*, ClassLoaderData*, ClassLoadInfo const*, ClassFileParser::Publicity, JavaThread*), ConstantPool::allocate_resolved_klasses(ClassLoaderData*, int, JavaThread*), Metaspace::allocate(ClassLoaderData*, unsigned long, MetaspaceObj::Type, JavaThread*) ... (12 total) |

## Arena Allocation Opportunities

Categories with >1% of samples that could benefit from arena allocation:

- **allocation**: 289.26% of samples
  - Functions: java/util/concurrent/FutureTask.runAndReset, java/util/HashMap$ValueSpliterator.tryAdvance, java/util/HashMap$HashMapSpliterator.getFence
- **conformation**: 43.25% of samples
  - Functions: org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection.visitChildren, org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection$UnresolvedItemsCollector.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration.visitContents
- **astar**: 17.63% of samples
  - Functions: Deoptimization::query_update_method_data(MethodData*, int, Deoptimization::DeoptReason, bool, bool, Method*, unsigned int&, bool&, bool&), KlassFactory::create_from_stream(ClassFileStream*, Symbol*, ClassLoaderData*, ClassLoadInfo const&, JavaThread*), ClassFileParser::ClassFileParser(ClassFileStream*, Symbol*, ClassLoaderData*, ClassLoadInfo const*, ClassFileParser::Publicity, JavaThread*)
