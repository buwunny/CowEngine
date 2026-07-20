#ifndef ENGINE_CONFIG_HPP
#define ENGINE_CONFIG_HPP

// Compile-time build-target switch.
//
//   ENGINE_WITH_EDITOR == 1  -> Editor build. The full ImGui/ImGuizmo editor
//                               (panels, gizmos, viewport tools, Build menu) is
//                               compiled and linked.
//   ENGINE_WITH_EDITOR == 0  -> Standalone / shipping runtime. No editor code,
//                               and crucially none of the UI library is
//                               compiled or linked into the binary.
//
// The value is normally supplied by the build system (see CMakeLists.txt, which
// defines ENGINE_WITH_EDITOR=1 for editor targets and =0 for standalone/game
// targets). The fallback below only guards against a misconfigured build so that
// `#if ENGINE_WITH_EDITOR` never expands to an error; it defaults to the
// full-featured editor build.
#ifndef ENGINE_WITH_EDITOR
#define ENGINE_WITH_EDITOR 1
#endif

#endif // ENGINE_CONFIG_HPP
