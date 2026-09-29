/**************************************************************************/
/*  godot_cef_input.h                                                     */
/**************************************************************************/

#pragma once

#include "godot_cef_include.h"

#include "core/input/input_event.h"

namespace GodotCefInput {

// Positions are local to the browser view in Godot units. They are converted to
// CEF view coordinates (DIP) with p_pixel_scale / p_device_scale.
struct Scale {
	float pixel = 1.0f;
	float device = 1.0f;
};

uint32_t get_key_modifiers(const Ref<InputEventWithModifiers> &p_event);
uint32_t get_mouse_button_modifiers(BitField<MouseButtonMask> p_mask);
CefMouseEvent make_mouse_event(const Vector2 &p_position, const Scale &p_scale, uint32_t p_modifiers);

void send_mouse_button(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventMouseButton> &p_event, const Scale &p_scale);
void send_mouse_motion(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventMouseMotion> &p_event, const Scale &p_scale);
void send_mouse_leave(CefRefPtr<CefBrowserHost> p_host, const Vector2 &p_position, const Scale &p_scale);
void send_pan_gesture(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventPanGesture> &p_event, const Scale &p_scale);
void send_magnify_gesture(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventMagnifyGesture> &p_event);
void send_screen_touch(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventScreenTouch> &p_event, const Scale &p_scale);
void send_screen_drag(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventScreenDrag> &p_event, const Scale &p_scale);

// p_ime_active: an editable element is focused and the OS IME delivers committed text
// as key events without a keycode.
void send_key(CefRefPtr<CefBrowser> p_browser, const Ref<InputEventKey> &p_event, bool p_ime_active);

void ime_set_composition(CefRefPtr<CefBrowserHost> p_host, const String &p_text, const Vector2i &p_selection);
void ime_commit_text(CefRefPtr<CefBrowserHost> p_host, const String &p_text);
void ime_cancel_composition(CefRefPtr<CefBrowserHost> p_host);

} // namespace GodotCefInput
