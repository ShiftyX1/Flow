/**************************************************************************/
/*  godot_cef_input_map.h                                                 */
/**************************************************************************/

#pragma once

#include "core/os/keyboard.h"

// Godot key -> CEF key event fields. CEF expects Windows virtual key codes in
// windows_key_code on every platform and the platform scan/virtual code in native_key_code.
namespace GodotCefInputMap {

int key_to_windows_keycode(Key p_key);
int key_to_macos_keycode(Key p_key);
int key_to_native_keycode(Key p_key);
char16_t key_to_control_char(Key p_key);

bool is_navigation_key(Key p_key);
bool is_modifier_key(Key p_key);
bool is_keypad_key(Key p_key);
bool should_send_char_event(Key p_key, char32_t p_unicode);

} // namespace GodotCefInputMap
