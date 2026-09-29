/**************************************************************************/
/*  test_godot_cef.h                                                      */
/**************************************************************************/

#pragma once

#include "../cef_texture.h"
#include "../cef_texture_2d.h"
#include "../godot_cef_data.h"
#include "../godot_cef_input_map.h"
#include "../godot_cef_paths.h"
#include "../godot_cef_scheme_util.h"
#include "../godot_cef_settings.h"

#include "core/object/class_db.h"
#include "tests/test_macros.h"

namespace TestGodotCef {

TEST_CASE("[GodotCEF] Settings expose stable project setting names and defaults") {
	CHECK(GodotCefSettings::SETTING_DATA_PATH == String("godot_cef/storage/data_path"));
	CHECK(GodotCefSettings::SETTING_REMOTE_DEVTOOLS_PORT == String("godot_cef/debug/remote_devtools_port"));
	CHECK(GodotCefSettings::SETTING_CUSTOM_SWITCHES == String("godot_cef/advanced/custom_command_line_switches"));

	CHECK(GodotCefSettings::DEFAULT_DATA_PATH == String("user://cef-data"));
	CHECK(GodotCefSettings::DEFAULT_REMOTE_DEVTOOLS_PORT == 9229);
	CHECK(GodotCefSettings::DEFAULT_MAX_FRAME_RATE == 0);
	CHECK(GodotCefSettings::DEFAULT_PERMISSION_POLICY == GodotCefSettings::PERMISSION_POLICY_DENY_ALL);
}

TEST_CASE("[GodotCEF] macOS runtime paths match the bundled CEF layout") {
	const String base = "/tmp/GodotCEF";
	const GodotCefRuntimePaths paths = GodotCefRuntimePaths::from_macos_runtime_dir(base);

	const String frameworks = base.path_join("Godot CEF.app/Contents/Frameworks");
	CHECK(paths.root_dir == base);
	CHECK(paths.main_bundle_path == base.path_join("Godot CEF.app"));
	CHECK(paths.framework_dir == frameworks.path_join("Chromium Embedded Framework.framework"));
	CHECK(paths.library_path == frameworks.path_join("Chromium Embedded Framework.framework/Chromium Embedded Framework"));
	CHECK(paths.subprocess_path == frameworks.path_join("Godot CEF Helper.app/Contents/MacOS/Godot CEF Helper"));
	CHECK_FALSE(paths.is_empty());
}

TEST_CASE("[GodotCEF] Windows runtime paths match the bundled CEF layout") {
	const String base = "C:/Game/GodotCEF";
	const GodotCefRuntimePaths paths = GodotCefRuntimePaths::from_windows_runtime_dir(base);

	CHECK(paths.root_dir == base);
	CHECK(paths.library_path == base.path_join("libcef.dll"));
	CHECK(paths.subprocess_path == base.path_join("godot_cef_helper.exe"));
	CHECK(paths.resources_dir == base);
	CHECK(paths.locales_dir == base.path_join("locales"));
}

TEST_CASE("[GodotCEF] Runtime lookup prefers the directory next to the executable") {
	const Vector<String> candidates = GodotCefRuntimePaths::get_candidate_runtime_dirs("/opt/game/game.x86_64");
	CHECK(candidates.has("/opt/game/GodotCEF"));
}

TEST_CASE("[GodotCEF] Scheme URLs map to sanitized Godot paths") {
	CHECK(GodotCefSchemeUtil::url_to_path("res://ui/index.html") == String("res://ui/index.html"));
	CHECK(GodotCefSchemeUtil::url_to_path("res://ui/index.html?v=1#top") == String("res://ui/index.html"));
	CHECK(GodotCefSchemeUtil::url_to_path("res://ui/my%20file.css") == String("res://ui/my file.css"));
	CHECK(GodotCefSchemeUtil::url_to_path("user://saves/./a/../b.json") == String("user://saves/b.json"));
	CHECK(GodotCefSchemeUtil::url_to_path("res://") == String("res://index.html"));
	CHECK(GodotCefSchemeUtil::url_to_path("res://ui/") == String("res://ui/index.html"));
	CHECK(GodotCefSchemeUtil::url_to_path("user://saves") == String("user://saves/index.html"));
	CHECK(GodotCefSchemeUtil::url_to_path("res://ui/../../etc/passwd") == String());
	CHECK(GodotCefSchemeUtil::url_to_path("res://c:/windows") == String());
	CHECK(GodotCefSchemeUtil::url_to_path("https://example.com/") == String());
}

TEST_CASE("[GodotCEF] MIME types and byte ranges") {
	CHECK(GodotCefSchemeUtil::get_mime_type("res://ui/index.HTML") == String("text/html"));
	CHECK(GodotCefSchemeUtil::get_mime_type("res://ui/app.js") == String("text/javascript"));
	CHECK(GodotCefSchemeUtil::get_mime_type("res://ui/module.wasm") == String("application/wasm"));
	CHECK(GodotCefSchemeUtil::get_mime_type("res://ui/blob.bin") == String("application/octet-stream"));

	int64_t start = 0;
	int64_t end = 0;
	CHECK(GodotCefSchemeUtil::parse_range("bytes=0-99", 1000, start, end));
	CHECK(start == 0);
	CHECK(end == 99);
	CHECK(GodotCefSchemeUtil::parse_range("bytes=900-", 1000, start, end));
	CHECK(start == 900);
	CHECK(end == 999);
	CHECK(GodotCefSchemeUtil::parse_range("bytes=-100", 1000, start, end));
	CHECK(start == 900);
	CHECK(end == 999);
	CHECK(GodotCefSchemeUtil::parse_range("bytes=500-5000", 1000, start, end));
	CHECK(end == 999);
	CHECK_FALSE(GodotCefSchemeUtil::parse_range("bytes=1000-", 1000, start, end));
	CHECK_FALSE(GodotCefSchemeUtil::parse_range("bytes=50-10", 1000, start, end));
	CHECK_FALSE(GodotCefSchemeUtil::parse_range("bytes=0-1,5-6", 1000, start, end));
	CHECK_FALSE(GodotCefSchemeUtil::parse_range("items=0-1", 1000, start, end));
}

TEST_CASE("[GodotCEF] Key mapping uses Windows virtual key codes") {
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::A) == 0x41);
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::KEY_0) == 0x30);
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::F1) == 0x70);
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::ENTER) == 0x0D);
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::BACKSPACE) == 0x08);
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::LEFT) == 0x25);
	CHECK(GodotCefInputMap::key_to_windows_keycode(Key::KP_5) == 0x65);

	CHECK(GodotCefInputMap::key_to_macos_keycode(Key::A) == 0x00);
	CHECK(GodotCefInputMap::key_to_macos_keycode(Key::ENTER) == 0x24);

	CHECK(GodotCefInputMap::key_to_control_char(Key::ENTER) == u'\r');
	CHECK(GodotCefInputMap::key_to_control_char(Key::BACKSPACE) == u'\b');

	CHECK(GodotCefInputMap::is_navigation_key(Key::LEFT));
	CHECK(GodotCefInputMap::is_modifier_key(Key::SHIFT));
	CHECK(GodotCefInputMap::should_send_char_event(Key::A, U'a'));
	CHECK(GodotCefInputMap::should_send_char_event(Key::ENTER, 0));
	CHECK_FALSE(GodotCefInputMap::should_send_char_event(Key::SHIFT, 0));
	CHECK_FALSE(GodotCefInputMap::should_send_char_event(Key::LEFT, 0));
}

TEST_CASE("[GodotCEF] Public classes are registered as native module classes") {
	CHECK(ClassDB::class_exists("CefTexture"));
	CHECK(ClassDB::class_exists("CefTexture2D"));
	CHECK(ClassDB::class_exists("CefIpcInspector"));
	CHECK(ClassDB::has_signal("CefTexture", "ipc_message"));
	CHECK(ClassDB::has_signal("CefTexture2D", "js_dialog_requested"));

	Ref<CefTexture2D> texture;
	texture.instantiate();
	CHECK(texture->get_url() == String("https://google.com"));
	CHECK(texture->get_texture_size() == Vector2i(1024, 1024));
	CHECK(texture->get_enable_accelerated_osr());
	CHECK_FALSE(texture->is_browser_ready());
	texture->set_texture_size(Vector2i(0, -5));
	CHECK(texture->get_texture_size() == Vector2i(1, 1));
	texture->set_popup_policy(42);
	CHECK(texture->get_popup_policy() == CefTexture2D::POPUP_POLICY_SIGNAL_ONLY);
}

TEST_CASE("[GodotCEF] Data objects mirror GDExtension payload defaults") {
	Ref<DragDataInfo> drag = DragDataInfo::create();
	CHECK(drag.is_valid());
	CHECK_FALSE(drag->get_is_link());
	CHECK_FALSE(drag->get_is_file());
	CHECK_FALSE(drag->get_is_fragment());
	CHECK(drag->get_file_names().is_empty());
	CHECK(DragOperation::EVERY == 2147483647);

	Ref<DownloadRequestInfo> request;
	request.instantiate();
	CHECK(request->get_id() == 0);
	CHECK(request->get_total_bytes() == -1);
	CHECK(request->get_original_url() == String());
	CHECK(request->get_mime_type() == String());

	Ref<DownloadUpdateInfo> update;
	update.instantiate();
	CHECK(update->get_total_bytes() == -1);
	CHECK(update->get_percent_complete() == -1);
	CHECK_FALSE(update->get_is_in_progress());
	CHECK_FALSE(update->get_is_complete());
	CHECK_FALSE(update->get_is_canceled());

	Ref<CookieInfo> cookie;
	cookie.instantiate();
	CHECK_FALSE(cookie->get_httponly());
	CHECK(cookie->get_same_site() == 0);
	CHECK_FALSE(cookie->get_has_expires());
}

} // namespace TestGodotCef
