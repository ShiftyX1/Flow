/**************************************************************************/
/*  godot_cef_input.cpp                                                   */
/**************************************************************************/

#include "godot_cef_input.h"

#include "godot_cef_input_map.h"
#include "godot_cef_ipc.h"

namespace GodotCefInput {

// Windows wheel delta for one notch, the unit CEF expects.
static constexpr float WHEEL_DELTA = 120.0f;
static constexpr double MAGNIFY_ZOOM_SENSITIVITY = 0.5;

uint32_t get_key_modifiers(const Ref<InputEventWithModifiers> &p_event) {
	uint32_t modifiers = EVENTFLAG_NONE;
	if (p_event.is_null()) {
		return modifiers;
	}
	if (p_event->is_shift_pressed()) {
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	}
	if (p_event->is_ctrl_pressed()) {
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	}
	if (p_event->is_alt_pressed()) {
		modifiers |= EVENTFLAG_ALT_DOWN;
	}
	if (p_event->is_meta_pressed()) {
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	}
	return modifiers;
}

uint32_t get_mouse_button_modifiers(BitField<MouseButtonMask> p_mask) {
	uint32_t modifiers = EVENTFLAG_NONE;
	if (p_mask.has_flag(MouseButtonMask::LEFT)) {
		modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	}
	if (p_mask.has_flag(MouseButtonMask::MIDDLE)) {
		modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	}
	if (p_mask.has_flag(MouseButtonMask::RIGHT)) {
		modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
	}
	return modifiers;
}

static Vector2 _to_view(const Vector2 &p_position, const Scale &p_scale) {
	const float device = p_scale.device > 0.0f ? p_scale.device : 1.0f;
	return p_position * p_scale.pixel / device;
}

CefMouseEvent make_mouse_event(const Vector2 &p_position, const Scale &p_scale, uint32_t p_modifiers) {
	const Vector2 view = _to_view(p_position, p_scale);
	CefMouseEvent event;
	event.x = int(Math::floor(view.x));
	event.y = int(Math::floor(view.y));
	event.modifiers = p_modifiers;
	return event;
}

void send_mouse_button(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventMouseButton> &p_event, const Scale &p_scale) {
	if (!p_host || p_event.is_null()) {
		return;
	}
	const uint32_t modifiers = get_key_modifiers(p_event) | get_mouse_button_modifiers(p_event->get_button_mask());
	const CefMouseEvent mouse_event = make_mouse_event(p_event->get_position(), p_scale, modifiers);
	const int delta = int(WHEEL_DELTA * (p_event->get_factor() > 0.0f ? p_event->get_factor() : 1.0f));

	switch (p_event->get_button_index()) {
		case MouseButton::LEFT:
		case MouseButton::MIDDLE:
		case MouseButton::RIGHT: {
			const MouseButton button = p_event->get_button_index();
			const cef_mouse_button_type_t type = button == MouseButton::LEFT ? MBT_LEFT : (button == MouseButton::MIDDLE ? MBT_MIDDLE : MBT_RIGHT);
			p_host->SendMouseClickEvent(mouse_event, type, !p_event->is_pressed(), p_event->is_double_click() ? 2 : 1);
		} break;
		case MouseButton::WHEEL_UP: {
			if (p_event->is_pressed()) {
				p_host->SendMouseWheelEvent(mouse_event, 0, delta);
			}
		} break;
		case MouseButton::WHEEL_DOWN: {
			if (p_event->is_pressed()) {
				p_host->SendMouseWheelEvent(mouse_event, 0, -delta);
			}
		} break;
		case MouseButton::WHEEL_LEFT: {
			if (p_event->is_pressed()) {
				p_host->SendMouseWheelEvent(mouse_event, -delta, 0);
			}
		} break;
		case MouseButton::WHEEL_RIGHT: {
			if (p_event->is_pressed()) {
				p_host->SendMouseWheelEvent(mouse_event, delta, 0);
			}
		} break;
		default:
			break;
	}
}

void send_mouse_motion(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventMouseMotion> &p_event, const Scale &p_scale) {
	if (!p_host || p_event.is_null()) {
		return;
	}
	const uint32_t modifiers = get_key_modifiers(p_event) | get_mouse_button_modifiers(p_event->get_button_mask());
	p_host->SendMouseMoveEvent(make_mouse_event(p_event->get_position(), p_scale, modifiers), false);
}

void send_mouse_leave(CefRefPtr<CefBrowserHost> p_host, const Vector2 &p_position, const Scale &p_scale) {
	if (!p_host) {
		return;
	}
	p_host->SendMouseMoveEvent(make_mouse_event(p_position, p_scale, EVENTFLAG_NONE), true);
}

void send_pan_gesture(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventPanGesture> &p_event, const Scale &p_scale) {
	if (!p_host || p_event.is_null()) {
		return;
	}
	const CefMouseEvent mouse_event = make_mouse_event(p_event->get_position(), p_scale, get_key_modifiers(p_event));
	const float device = p_scale.device > 0.0f ? p_scale.device : 1.0f;
	const Vector2 delta = p_event->get_delta();
	const int delta_x = int(-delta.x * WHEEL_DELTA / device);
	const int delta_y = int(-delta.y * WHEEL_DELTA / device);
	if (delta_x != 0 || delta_y != 0) {
		p_host->SendMouseWheelEvent(mouse_event, delta_x, delta_y);
	}
}

void send_magnify_gesture(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventMagnifyGesture> &p_event) {
	if (!p_host || p_event.is_null()) {
		return;
	}
	const double factor = p_event->get_factor() - 1.0;
	if (Math::abs(factor) <= CMP_EPSILON) {
		return;
	}
	p_host->SetZoomLevel(p_host->GetZoomLevel() + factor * MAGNIFY_ZOOM_SENSITIVITY);
}

static CefTouchEvent _make_touch(const Vector2 &p_position, const Scale &p_scale, cef_touch_event_type_t p_type, int p_id, float p_pressure) {
	const Vector2 view = _to_view(p_position, p_scale);
	CefTouchEvent event;
	event.id = p_id;
	event.x = view.x;
	event.y = view.y;
	event.pressure = CLAMP(p_pressure, 0.0f, 1.0f);
	event.type = p_type;
	event.pointer_type = CEF_POINTER_TYPE_TOUCH;
	return event;
}

void send_screen_touch(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventScreenTouch> &p_event, const Scale &p_scale) {
	if (!p_host || p_event.is_null()) {
		return;
	}
	cef_touch_event_type_t type = CEF_TET_RELEASED;
	if (p_event->is_canceled()) {
		type = CEF_TET_CANCELLED;
	} else if (p_event->is_pressed()) {
		type = CEF_TET_PRESSED;
	}
	p_host->SendTouchEvent(_make_touch(p_event->get_position(), p_scale, type, p_event->get_index(), 1.0f));
}

void send_screen_drag(CefRefPtr<CefBrowserHost> p_host, const Ref<InputEventScreenDrag> &p_event, const Scale &p_scale) {
	if (!p_host || p_event.is_null()) {
		return;
	}
	p_host->SendTouchEvent(_make_touch(p_event->get_position(), p_scale, CEF_TET_MOVED, p_event->get_index(), p_event->get_pressure()));
}

// Editing shortcuts that windowless browsers do not handle on their own.
static bool _handle_edit_shortcut(CefRefPtr<CefFrame> p_frame, const Ref<InputEventKey> &p_event) {
	if (!p_frame || !p_event->is_command_or_control_pressed() || p_event->is_alt_pressed()) {
		return false;
	}
	const bool shift = p_event->is_shift_pressed();
	switch (p_event->get_keycode()) {
		case Key::A:
			if (!shift) {
				p_frame->SelectAll();
				return true;
			}
			break;
		case Key::C:
			if (!shift) {
				p_frame->Copy();
				return true;
			}
			break;
		case Key::X:
			if (!shift) {
				p_frame->Cut();
				return true;
			}
			break;
		case Key::V:
			if (shift) {
				p_frame->PasteAndMatchStyle();
				return true;
			}
			break;
#ifdef MACOS_ENABLED
		case Key::Z:
			if (shift) {
				p_frame->Redo();
			} else {
				p_frame->Undo();
			}
			return true;
#endif
		default:
			break;
	}
	return false;
}

static void _send_char(CefRefPtr<CefBrowserHost> p_host, char16_t p_character, uint32_t p_modifiers) {
	CefKeyEvent char_event;
	char_event.type = KEYEVENT_CHAR;
	char_event.modifiers = p_modifiers;
	// Like WM_CHAR, the key code of a CHAR event is the character itself.
	char_event.windows_key_code = p_character;
	char_event.native_key_code = p_character;
	char_event.character = p_character;
	char_event.unmodified_character = p_character;
	p_host->SendKeyEvent(char_event);
}

void send_key(CefRefPtr<CefBrowser> p_browser, const Ref<InputEventKey> &p_event, bool p_ime_active) {
	if (!p_browser || p_event.is_null()) {
		return;
	}
	CefRefPtr<CefBrowserHost> host = p_browser->GetHost();
	const Key keycode = p_event->get_keycode();
	const char32_t unicode = p_event->get_unicode();
	uint32_t modifiers = get_key_modifiers(p_event);

	if (keycode == Key::NONE) {
		// Text produced by the OS text input system (IME commit or dead keys).
		if (!p_event->is_pressed() || unicode == 0) {
			return;
		}
		if (p_ime_active) {
			ime_commit_text(host, String::chr(unicode));
		} else if (unicode <= 0xFFFF) {
			_send_char(host, char16_t(unicode), modifiers);
		}
		return;
	}

	if (GodotCefInputMap::is_keypad_key(p_event->get_physical_keycode() != Key::NONE ? p_event->get_physical_keycode() : keycode)) {
		modifiers |= EVENTFLAG_IS_KEY_PAD;
	}

	const bool pressed = p_event->is_pressed();
	const bool echo = p_event->is_echo();
	if (pressed && !echo && _handle_edit_shortcut(p_browser->GetFocusedFrame(), p_event)) {
		return;
	}

	const Key native_key = p_event->get_physical_keycode() != Key::NONE ? p_event->get_physical_keycode() : keycode;
	char16_t character = unicode > 0 && unicode <= 0xFFFF ? char16_t(unicode) : GodotCefInputMap::key_to_control_char(keycode);

	CefKeyEvent key_event;
	key_event.modifiers = modifiers;
	key_event.windows_key_code = GodotCefInputMap::key_to_windows_keycode(keycode);
	key_event.native_key_code = GodotCefInputMap::key_to_native_keycode(native_key);
	key_event.character = character;
	key_event.unmodified_character = character;
	key_event.focus_on_editable_field = p_ime_active;

	if (pressed) {
		key_event.type = echo ? KEYEVENT_KEYDOWN : KEYEVENT_RAWKEYDOWN;
		host->SendKeyEvent(key_event);
		if (character != 0 && GodotCefInputMap::should_send_char_event(keycode, unicode) && !(modifiers & (EVENTFLAG_CONTROL_DOWN | EVENTFLAG_COMMAND_DOWN))) {
			_send_char(host, character, modifiers);
		}
	} else if (!GodotCefInputMap::is_navigation_key(keycode)) {
		// KEYUP for navigation keys repeats the arrow action on macOS.
		key_event.type = KEYEVENT_KEYUP;
		host->SendKeyEvent(key_event);
	}
}

static int _utf16_length(const String &p_text, int p_codepoints) {
	int length = 0;
	const int count = MIN(p_codepoints, p_text.length());
	for (int i = 0; i < count; i++) {
		length += p_text[i] > 0xFFFF ? 2 : 1;
	}
	return length;
}

void ime_set_composition(CefRefPtr<CefBrowserHost> p_host, const String &p_text, const Vector2i &p_selection) {
	if (!p_host) {
		return;
	}
	const uint32_t text_length = uint32_t(_utf16_length(p_text, p_text.length()));
	CefCompositionUnderline underline;
	underline.range = CefRange(0, text_length);
	underline.color = 0;
	underline.background_color = 0;
	underline.thick = 0;
	underline.style = CEF_CUS_SOLID;
	std::vector<CefCompositionUnderline> underlines = { underline };

	// Godot reports (start, length) in codepoints; CEF wants a UTF-16 range.
	const uint32_t start = uint32_t(_utf16_length(p_text, MAX(0, p_selection.x)));
	const uint32_t end = uint32_t(_utf16_length(p_text, MAX(0, p_selection.x + MAX(0, p_selection.y))));
	p_host->ImeSetComposition(GodotCefIpcData::to_cef_string(p_text), underlines, CefRange::InvalidRange(), CefRange(start, end));
}

void ime_commit_text(CefRefPtr<CefBrowserHost> p_host, const String &p_text) {
	if (!p_host || p_text.is_empty()) {
		return;
	}
	p_host->ImeCommitText(GodotCefIpcData::to_cef_string(p_text), CefRange::InvalidRange(), 0);
}

void ime_cancel_composition(CefRefPtr<CefBrowserHost> p_host) {
	if (p_host) {
		p_host->ImeCancelComposition();
	}
}

} // namespace GodotCefInput
