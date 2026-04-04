---
paths: "src/**/*.cpp, src/**/*.h, src/**/*.hpp, CMakeLists.txt, CMakePresets.json"
---

# VoxCore C++ Coding Conventions

- **C++ standard**: C++20 features OK (structured bindings, `contains()`, `string_view`, etc.)
- **Header guards**: `#pragma once` for new files
- **Indent**: 4 spaces, **Max line**: 160, **Charset**: latin1 (see `.editorconfig`)
- **Visibility**: Use `TC_GAME_API` on classes in `src/server/game/`
- **Singletons**: Static local instance pattern, exposed via `sFoo` macro
- **Script registration**: `void AddSC_<name>()` free function, registered in `custom_script_loader.cpp`
- **Spell scripts**: `RegisterSpellScript(ClassName)` macro. Others: `new ClassName()` auto-registers
- **Namespaces**: `RoleplayCore::` (display), `Noblegarden::` (effects)
- **RBAC**: Custom permissions in `1000+` / `2100+` / `3000+` ranges
- **Includes**: `#include "..."` for TC headers, `#include <...>` for system
