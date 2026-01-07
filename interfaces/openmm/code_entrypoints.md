# OpenMM: code entry points 

Repo: `interfaces/openmm`

## Core object model

- **System**: `interfaces/openmm/openmmapi/include/openmm/System.h`
- **Force (term front-end)**: `interfaces/openmm/openmmapi/include/openmm/Force.h`
  - Term object creates an internal implementation via `Force::createImpl()` (front-end/back-end split).
- **Context**: `interfaces/openmm/openmmapi/include/openmm/Context.h`
- **Platform (backend registry)**: `interfaces/openmm/olla/include/openmm/Platform.h`
  - Platform is a registry of `KernelFactory` implementations and is selected via `Platform::findPlatform(...)`.
- **Kernel (backend unit)**: `interfaces/openmm/olla/include/openmm/Kernel.h`

## Extension points to study

- **New energy term**: implement a new `Force` subclass + matching `ForceImpl`.
- **New backend**: implement a `Platform` with `KernelFactory` registrations for required kernels.
- **Custom terms**: look for `Custom*Force` classes in `openmmapi/include/openmm/` as a built-in “user-defined potential” mechanism.

## Docs build entry points (from DOC_INDEX)

- Doxygen/Sphinx sources: `interfaces/openmm/docs-source/`
  - C++ API: `interfaces/openmm/docs-source/api-c++/Doxyfile.in`, `interfaces/openmm/docs-source/api-c++/conf.py`
  - Python API: `interfaces/openmm/docs-source/api-python/conf.py`
  - User/developer: `interfaces/openmm/docs-source/usersguide/conf.py`, `interfaces/openmm/docs-source/developerguide/conf.py`

