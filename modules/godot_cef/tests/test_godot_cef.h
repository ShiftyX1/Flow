/**************************************************************************/
/*  test_godot_cef.h                                                      */
/**************************************************************************/

#pragma once

#include "../cef_texture.h"
#include "../cef_texture_2d.h"
#include "../godot_cef_data.h"
#include "../godot_cef_paths.h"
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

TEST_CASE("[GodotCEF] macOS runtime paths match bundled CEF layout") {
	const String base = "/tmp/Godot CEF Runtime";
	const GodotCefRuntimePaths paths = GodotCefRuntimePaths::from_macos_universal_dir(base);

	CHECK(paths.root_dir == base);
	CHECK(paths.framework_dir == base.path_join("Godot CEF.app/Contents/Frameworks"));
	CHECK(paths.subprocess_path == base.path_join("Godot CEF.app/Contents/Frameworks/Godot CEF Helper.app/Contents/MacOS/Godot CEF Helper"));
	CHECK(paths.library_path == base.path_join("Godot CEF.framework/libgdcef.dylib"));
	CHECK(paths.is_valid_for_macos());
}

TEST_CASE("[GodotCEF] Public classes are registered as native module classes") {
	CHECK(ClassDB::class_exists("CefTexture"));
	CHECK(ClassDB::class_exists("CefTexture2D"));
	CHECK(ClassDB::class_exists("CefIpcInspector"));

	Ref<CefTexture2D> texture;
	texture.instantiate();
	CHECK(texture->get_url() == String("https://google.com"));
	CHECK(texture->get_texture_size() == Vector2i(1024, 1024));
	CHECK(texture->get_enable_accelerated_osr());
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
