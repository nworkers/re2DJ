#ifndef RE2DJ_PLATFORM_WINDOWS_KEYBOARD_INPUT_COMMON_H_
#define RE2DJ_PLATFORM_WINDOWS_KEYBOARD_INPUT_COMMON_H_

#include <string>

namespace re2dj::platform::windows
{

bool ReadKeyboardKeyBinding(const char* path,
                            const char* section,
                            const char* name,
                            int* key,
                            std::string* error);
bool IsKeyboardKeyPressed(int key);

}  // namespace re2dj::platform::windows

#endif  // RE2DJ_PLATFORM_WINDOWS_KEYBOARD_INPUT_COMMON_H_
