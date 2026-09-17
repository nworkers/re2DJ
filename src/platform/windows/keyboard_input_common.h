#ifndef RE2DJ_PLATFORM_WINDOWS_KEYBOARD_INPUT_COMMON_H_
#define RE2DJ_PLATFORM_WINDOWS_KEYBOARD_INPUT_COMMON_H_

#include <string>

namespace re2dj::platform::windows
{

// Reads one key binding from an INI file, reporting through `present` whether
// the file held that entry at all. A caller keeps its built-in default when it
// did not; an entry written as NONE is present and unbinds the key.
bool ReadKeyboardKeyBinding(const char* path,
                            const char* section,
                            const char* name,
                            int* key,
                            bool* present,
                            std::string* error);
// Interprets a key name as the binding tables and INI files write it.
bool ParseKeyboardKeyName(const char* name, int* key);
bool IsKeyboardKeyPressed(int key);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_KEYBOARD_INPUT_COMMON_H_
