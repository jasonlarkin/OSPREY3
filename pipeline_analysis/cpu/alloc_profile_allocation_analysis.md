# ALLOC_PROFILE_ALLOCATION Profile Analysis

Total samples: 464

## Top Functions

| Function | Samples | Percentage |
|----------|---------|------------|
| `org/gradle/internal/operations/DefaultBuildOperationRunner.execute` | 1,306 | 281.47% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner$2.execute` | 1,306 | 281.47% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner$CallableBuildOperatio` | 1,002 | 215.95% |
| `groovyjarjarantlr4/v4/runtime/atn/ParserATNSimulator.closure` | 724 | 156.03% |
| `org/gradle/internal/concurrent/ExecutorPolicy$CatchAndRecordFailures.onExecute` | 600 | 129.31% |
| `org/gradle/internal/operations/DefaultBuildOperationExecutor.call` | 501 | 107.97% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner.call` | 501 | 107.97% |
| `all` | 464 | 100.00% |
| `java/lang/Thread.run` | 460 | 99.14% |
| `org/gradle/internal/concurrent/ThreadFactoryImpl$ManagedThreadRunnable.run` | 457 | 98.49% |
| `java/util/concurrent/ThreadPoolExecutor$Worker.run` | 457 | 98.49% |
| `java/util/concurrent/ThreadPoolExecutor.runWorker` | 457 | 98.49% |
| `org/gradle/internal/concurrent/ManagedExecutorImpl$1.run` | 451 | 97.20% |
| `org/gradle/api/internal/file/copy/DefaultCopySpec$DefaultCopySpecResolver.walk` | 447 | 96.34% |
| `org/gradle/execution/taskgraph/DefaultTaskExecutionGraph$BuildOperationAwareExec` | 412 | 88.79% |
| `org/gradle/execution/taskgraph/DefaultTaskExecutionGraph$InvokeNodeExecutorsActi` | 412 | 88.79% |
| `org/gradle/api/internal/tasks/execution/EventFiringTaskExecuter$1.call` | 412 | 88.79% |
| `org/gradle/internal/execution/steps/IdentifyStep.execute` | 412 | 88.79% |
| `org/gradle/internal/execution/steps/IdentityCacheStep.execute` | 412 | 88.79% |
| `org/gradle/internal/execution/steps/AssignWorkspaceStep.execute` | 412 | 88.79% |
| `org/gradle/internal/execution/steps/LoadPreviousExecutionStateStep.execute` | 412 | 88.79% |
| `org/gradle/internal/execution/steps/SkipEmptyWorkStep.execute` | 412 | 88.79% |
| `org/gradle/internal/execution/steps/CaptureStateBeforeExecutionStep.execute` | 412 | 88.79% |
| `org/gradle/cache/internal/DefaultCacheAccess.useCache` | 384 | 82.76% |
| `java/util/Optional.orElseGet` | 371 | 79.96% |
| `java/lang/ClassLoader.loadClass` | 311 | 67.03% |
| `org/gradle/internal/execution/steps/ValidateStep.execute` | 310 | 66.81% |
| `org/gradle/internal/operations/DefaultBuildOperationRunner$1.execute` | 304 | 65.52% |
| `org/gradle/internal/execution/steps/ResolveCachingStateStep.execute` | 302 | 65.09% |
| `org/gradle/internal/execution/steps/legacy/MarkSnapshottingInputsFinishedStep.ex` | 302 | 65.09% |

## OSPREY-Specific Hotspots

| Category | Samples | Percentage | Functions |
|----------|---------|------------|-----------|
| gc | 4,468 | 962.93% | org/gradle/launcher/daemon/server/health/gc/GarbageCollectionCheck.run, java/util/stream/ReferencePipeline.collect, java/util/stream/Collectors$$Lambda$59.0x00007fd36706d4f0.get, org/gradle/internal/execution/history/impl/FileCollectionFingerprintSerializer.read, org/gradle/internal/execution/history/impl/FileCollectionFingerprintSerializer.readRootHashes ... (135 total) |
| allocation | 1,114 | 240.09% | java/util/concurrent/FutureTask.runAndReset, java/util/HashMap$ValueSpliterator.forEachRemaining, java/lang/management/DefaultPlatformMBeanProvider$6.nameToMBeanMap, java/util/Arrays.copyOfRange, java.util.ArrayList ... (162 total) |
| conformation | 934 | 201.29% | org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection.visitChildren, org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection$UnresolvedItemsCollector.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration$ConfigurationFileCollection.visitContents, org/gradle/api/internal/artifacts/ivyservice/DefaultLenientConfiguration$1.visitArtifacts ... (57 total) |

## Arena Allocation Opportunities

Categories with >1% of samples that could benefit from arena allocation:

- **allocation**: 240.09% of samples
  - Functions: java/util/concurrent/FutureTask.runAndReset, java/util/HashMap$ValueSpliterator.forEachRemaining, java/lang/management/DefaultPlatformMBeanProvider$6.nameToMBeanMap
- **conformation**: 201.29% of samples
  - Functions: org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection.visitChildren, org/gradle/api/internal/file/collections/DefaultConfigurableFileCollection$UnresolvedItemsCollector.visitContents, org/gradle/api/internal/artifacts/configurations/DefaultConfiguration.visitContents
