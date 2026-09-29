/**************************************************************************/
/*  godot_cef_input_map.cpp                                               */
/**************************************************************************/

#include "godot_cef_input_map.h"

namespace GodotCefInputMap {

int key_to_windows_keycode(Key p_key) {
	if (p_key >= Key::A && p_key <= Key::Z) {
		return 0x41 + int(p_key) - int(Key::A);
	}
	if (p_key >= Key::KEY_0 && p_key <= Key::KEY_9) {
		return 0x30 + int(p_key) - int(Key::KEY_0);
	}
	if (p_key >= Key::F1 && p_key <= Key::F24) {
		return 0x70 + int(p_key) - int(Key::F1);
	}
	if (p_key >= Key::KP_0 && p_key <= Key::KP_9) {
		return 0x60 + int(p_key) - int(Key::KP_0);
	}

	switch (p_key) {
		case Key::KP_MULTIPLY:
			return 0x6A;
		case Key::KP_ADD:
			return 0x6B;
		case Key::KP_SUBTRACT:
			return 0x6D;
		case Key::KP_PERIOD:
			return 0x6E;
		case Key::KP_DIVIDE:
			return 0x6F;
		case Key::BACKSPACE:
			return 0x08;
		case Key::TAB:
		case Key::BACKTAB:
			return 0x09;
		case Key::CLEAR:
			return 0x0C;
		case Key::ENTER:
		case Key::KP_ENTER:
			return 0x0D;
		case Key::SHIFT:
			return 0x10;
		case Key::CTRL:
			return 0x11;
		case Key::ALT:
			return 0x12;
		case Key::PAUSE:
			return 0x13;
		case Key::CAPSLOCK:
			return 0x14;
		case Key::ESCAPE:
			return 0x1B;
		case Key::SPACE:
			return 0x20;
		case Key::PAGEUP:
			return 0x21;
		case Key::PAGEDOWN:
			return 0x22;
		case Key::END:
			return 0x23;
		case Key::HOME:
			return 0x24;
		case Key::LEFT:
			return 0x25;
		case Key::UP:
			return 0x26;
		case Key::RIGHT:
			return 0x27;
		case Key::DOWN:
			return 0x28;
		case Key::PRINT:
			return 0x2C;
		case Key::INSERT:
			return 0x2D;
		case Key::KEY_DELETE:
			return 0x2E;
		case Key::HELP:
			return 0x2F;
		case Key::META:
			return 0x5B;
		case Key::MENU:
			return 0x5D;
		case Key::NUMLOCK:
			return 0x90;
		case Key::SCROLLLOCK:
			return 0x91;
		case Key::SEMICOLON:
			return 0xBA;
		case Key::EQUAL:
			return 0xBB;
		case Key::COMMA:
			return 0xBC;
		case Key::MINUS:
			return 0xBD;
		case Key::PERIOD:
			return 0xBE;
		case Key::SLASH:
			return 0xBF;
		case Key::QUOTELEFT:
			return 0xC0;
		case Key::BRACKETLEFT:
			return 0xDB;
		case Key::BACKSLASH:
			return 0xDC;
		case Key::BRACKETRIGHT:
			return 0xDD;
		case Key::APOSTROPHE:
			return 0xDE;
		default:
			return 0;
	}
}

int key_to_macos_keycode(Key p_key) {
	// HIToolbox/Events.h kVK_* values.
	switch (p_key) {
		case Key::A:
			return 0x00;
		case Key::S:
			return 0x01;
		case Key::D:
			return 0x02;
		case Key::F:
			return 0x03;
		case Key::H:
			return 0x04;
		case Key::G:
			return 0x05;
		case Key::Z:
			return 0x06;
		case Key::X:
			return 0x07;
		case Key::C:
			return 0x08;
		case Key::V:
			return 0x09;
		case Key::B:
			return 0x0B;
		case Key::Q:
			return 0x0C;
		case Key::W:
			return 0x0D;
		case Key::E:
			return 0x0E;
		case Key::R:
			return 0x0F;
		case Key::Y:
			return 0x10;
		case Key::T:
			return 0x11;
		case Key::KEY_1:
			return 0x12;
		case Key::KEY_2:
			return 0x13;
		case Key::KEY_3:
			return 0x14;
		case Key::KEY_4:
			return 0x15;
		case Key::KEY_6:
			return 0x16;
		case Key::KEY_5:
			return 0x17;
		case Key::EQUAL:
			return 0x18;
		case Key::KEY_9:
			return 0x19;
		case Key::KEY_7:
			return 0x1A;
		case Key::MINUS:
			return 0x1B;
		case Key::KEY_8:
			return 0x1C;
		case Key::KEY_0:
			return 0x1D;
		case Key::BRACKETRIGHT:
			return 0x1E;
		case Key::O:
			return 0x1F;
		case Key::U:
			return 0x20;
		case Key::BRACKETLEFT:
			return 0x21;
		case Key::I:
			return 0x22;
		case Key::P:
			return 0x23;
		case Key::ENTER:
			return 0x24;
		case Key::L:
			return 0x25;
		case Key::J:
			return 0x26;
		case Key::APOSTROPHE:
			return 0x27;
		case Key::K:
			return 0x28;
		case Key::SEMICOLON:
			return 0x29;
		case Key::BACKSLASH:
			return 0x2A;
		case Key::COMMA:
			return 0x2B;
		case Key::SLASH:
			return 0x2C;
		case Key::N:
			return 0x2D;
		case Key::M:
			return 0x2E;
		case Key::PERIOD:
			return 0x2F;
		case Key::TAB:
		case Key::BACKTAB:
			return 0x30;
		case Key::SPACE:
			return 0x31;
		case Key::QUOTELEFT:
			return 0x32;
		case Key::BACKSPACE:
			return 0x33;
		case Key::ESCAPE:
			return 0x35;
		case Key::META:
			return 0x37;
		case Key::SHIFT:
			return 0x38;
		case Key::CAPSLOCK:
			return 0x39;
		case Key::ALT:
			return 0x3A;
		case Key::CTRL:
			return 0x3B;
		case Key::KP_PERIOD:
			return 0x41;
		case Key::KP_MULTIPLY:
			return 0x43;
		case Key::KP_ADD:
			return 0x45;
		case Key::KP_DIVIDE:
			return 0x4B;
		case Key::KP_ENTER:
			return 0x4C;
		case Key::KP_SUBTRACT:
			return 0x4E;
		case Key::KP_0:
			return 0x52;
		case Key::KP_1:
			return 0x53;
		case Key::KP_2:
			return 0x54;
		case Key::KP_3:
			return 0x55;
		case Key::KP_4:
			return 0x56;
		case Key::KP_5:
			return 0x57;
		case Key::KP_6:
			return 0x58;
		case Key::KP_7:
			return 0x59;
		case Key::KP_8:
			return 0x5B;
		case Key::KP_9:
			return 0x5C;
		case Key::F5:
			return 0x60;
		case Key::F6:
			return 0x61;
		case Key::F7:
			return 0x62;
		case Key::F3:
			return 0x63;
		case Key::F8:
			return 0x64;
		case Key::F9:
			return 0x65;
		case Key::F11:
			return 0x67;
		case Key::F10:
			return 0x6D;
		case Key::F12:
			return 0x6F;
		case Key::HELP:
		case Key::INSERT:
			return 0x72;
		case Key::HOME:
			return 0x73;
		case Key::PAGEUP:
			return 0x74;
		case Key::KEY_DELETE:
			return 0x75;
		case Key::F4:
			return 0x76;
		case Key::END:
			return 0x77;
		case Key::F2:
			return 0x78;
		case Key::PAGEDOWN:
			return 0x79;
		case Key::F1:
			return 0x7A;
		case Key::LEFT:
			return 0x7B;
		case Key::RIGHT:
			return 0x7C;
		case Key::DOWN:
			return 0x7D;
		case Key::UP:
			return 0x7E;
		default:
			return 0;
	}
}

int key_to_native_keycode(Key p_key) {
#ifdef MACOS_ENABLED
	return key_to_macos_keycode(p_key);
#else
	return key_to_windows_keycode(p_key);
#endif
}

char16_t key_to_control_char(Key p_key) {
	switch (p_key) {
		case Key::BACKSPACE:
			return 0x08;
		case Key::TAB:
			return 0x09;
		case Key::ENTER:
		case Key::KP_ENTER:
			return 0x0D;
		case Key::ESCAPE:
			return 0x1B;
		case Key::KEY_DELETE:
			return 0x7F;
		default:
			return 0;
	}
}

bool is_navigation_key(Key p_key) {
	switch (p_key) {
		case Key::UP:
		case Key::DOWN:
		case Key::LEFT:
		case Key::RIGHT:
		case Key::HOME:
		case Key::END:
		case Key::PAGEUP:
		case Key::PAGEDOWN:
			return true;
		default:
			return false;
	}
}

bool is_modifier_key(Key p_key) {
	switch (p_key) {
		case Key::SHIFT:
		case Key::CTRL:
		case Key::ALT:
		case Key::META:
		case Key::CAPSLOCK:
		case Key::NUMLOCK:
		case Key::SCROLLLOCK:
			return true;
		default:
			return false;
	}
}

bool is_keypad_key(Key p_key) {
	if (p_key >= Key::KP_0 && p_key <= Key::KP_9) {
		return true;
	}
	switch (p_key) {
		case Key::KP_MULTIPLY:
		case Key::KP_SUBTRACT:
		case Key::KP_PERIOD:
		case Key::KP_ADD:
		case Key::KP_DIVIDE:
		case Key::KP_ENTER:
			return true;
		default:
			return false;
	}
}

bool should_send_char_event(Key p_key, char32_t p_unicode) {
	if (is_modifier_key(p_key) || is_navigation_key(p_key)) {
		return false;
	}
	return p_unicode != 0 || key_to_control_char(p_key) != 0;
}

} // namespace GodotCefInputMap
