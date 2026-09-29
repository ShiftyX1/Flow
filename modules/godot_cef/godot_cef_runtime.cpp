/**************************************************************************/
/*  godot_cef_runtime.cpp                                                 */
/**************************************************************************/

#include "godot_cef_runtime.h"

#include "cef_texture_2d.h"
#include "godot_cef_app.h"
#include "godot_cef_include.h"
#include "godot_cef_paths.h"
#include "godot_cef_settings.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/templates/local_vector.h"
#include "scene/main/scene_tree.h"

#ifdef MACOS_ENABLED
#include "include/wrapper/cef_library_loader.h"
#endif

#ifdef WINDOWS_ENABLED
#include <windows.h>
#endif

#include <atomic>

namespace {

constexpr uint64_t PUMP_NOT_SCHEDULED = UINT64_MAX;
// CEF recommends pumping periodically even without scheduled work.
constexpr uint64_t PUMP_FALLBACK_INTERVAL_USEC = 33333;
constexpr uint64_t SHUTDOWN_TIMEOUT_USEC = 3000000;

GodotCefRuntime::State state = GodotCefRuntime::STATE_UNINITIALIZED;
String last_error;
LocalVector<ObjectID> textures;
ObjectID hooked_tree;
int open_browsers = 0;
bool library_loaded = false;
uint64_t last_pump_usec = 0;
std::atomic<uint64_t> pump_due_usec{ PUMP_NOT_SCHEDULED };

#ifdef MACOS_ENABLED
CharString macos_argv0;
char *macos_argv[2] = { nullptr, nullptr };
#endif

void _fail(const String &p_error) {
	state = GodotCefRuntime::STATE_FAILED;
	last_error = p_error;
	ERR_PRINT("Godot CEF: " + p_error);
}

String _get_cache_root() {
	String data_path = GodotCefSettings::get_data_path();
	if (data_path.is_empty()) {
		data_path = GodotCefSettings::DEFAULT_DATA_PATH;
	}
	String root = ProjectSettings::get_singleton()->globalize_path(data_path);
	// The editor and a running project must not share a Chromium profile directory.
	if (Engine::get_singleton()->is_editor_hint()) {
		root = root.path_join("editor");
	}
	return root;
}

void _pump() {
	pump_due_usec.store(PUMP_NOT_SCHEDULED);
	last_pump_usec = OS::get_singleton()->get_ticks_usec();
	CefDoMessageLoopWork();
}

void _unload_library() {
	if (!library_loaded) {
		return;
	}
#ifdef MACOS_ENABLED
	cef_unload_library();
#endif
	// On Windows libcef.dll stays loaded: the delay-load thunks keep pointing into it.
	library_loaded = false;
}

bool _load_library(const GodotCefRuntimePaths &p_paths) {
#ifdef MACOS_ENABLED
	if (!cef_load_library(p_paths.library_path.utf8().get_data())) {
		_fail(vformat("Failed to load the CEF framework from \"%s\".", p_paths.library_path));
		return false;
	}
#elif defined(WINDOWS_ENABLED)
	// libcef.dll is delay-loaded; loading it by full path first makes the delay-load helper
	// resolve to this module and lets its dependencies be found next to it.
	HMODULE module = LoadLibraryExW((LPCWSTR)p_paths.library_path.utf16().get_data(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!module) {
		_fail(vformat("Failed to load \"%s\" (error %d).", p_paths.library_path, int(GetLastError())));
		return false;
	}
#else
	_fail("Godot CEF is only supported on macOS and Windows.");
	return false;
#endif
	library_loaded = true;
	return true;
}

} // namespace

void GodotCefRuntime::initialize() {
	if (FileAccess::exists("res://addons/godot_cef/godot_cef.gdextension")) {
		WARN_PRINT("Godot CEF native module is enabled, but res://addons/godot_cef/godot_cef.gdextension is present. Remove the old GDExtension addon to avoid duplicate Godot CEF integration.");
	}
}

Error GodotCefRuntime::ensure_initialized() {
	switch (state) {
		case STATE_INITIALIZED:
			return OK;
		case STATE_FAILED:
			return ERR_UNAVAILABLE;
		case STATE_SHUT_DOWN:
			return ERR_UNAVAILABLE;
		case STATE_UNINITIALIZED:
			break;
	}
	ERR_FAIL_COND_V_MSG(!Thread::is_main_thread(), ERR_UNAVAILABLE, "Godot CEF must be initialized from the main thread.");

	const GodotCefRuntimePaths paths = GodotCefRuntimePaths::detect(OS::get_singleton()->get_executable_path());
	if (paths.is_empty()) {
		String searched;
		for (const String &dir : GodotCefRuntimePaths::get_candidate_runtime_dirs(OS::get_singleton()->get_executable_path())) {
			searched += "\n  " + dir;
		}
		_fail(vformat("CEF runtime not found. Build with godot_cef_bundle_runtime=yes or set %s. Searched:%s", GodotCefRuntimePaths::RUNTIME_DIR_ENV, searched));
		return ERR_FILE_NOT_FOUND;
	}
	if (!_load_library(paths)) {
		return ERR_CANT_OPEN;
	}

#ifdef MACOS_ENABLED
	macos_argv0 = OS::get_singleton()->get_executable_path().utf8();
	macos_argv[0] = macos_argv0.ptrw();
	CefMainArgs main_args(1, macos_argv);
#elif defined(WINDOWS_ENABLED)
	CefMainArgs main_args(GetModuleHandleW(nullptr));
#else
	CefMainArgs main_args;
#endif

	CefSettings settings;
	settings.no_sandbox = true;
	settings.windowless_rendering_enabled = true;
	settings.external_message_pump = true;
	settings.multi_threaded_message_loop = false;
	settings.persist_session_cookies = true;
	CefString(&settings.browser_subprocess_path).FromString(paths.subprocess_path.utf8().get_data());
#ifdef MACOS_ENABLED
	CefString(&settings.framework_dir_path).FromString(paths.framework_dir.utf8().get_data());
	CefString(&settings.main_bundle_path).FromString(paths.main_bundle_path.utf8().get_data());
#else
	CefString(&settings.resources_dir_path).FromString(paths.resources_dir.utf8().get_data());
	CefString(&settings.locales_dir_path).FromString(paths.locales_dir.utf8().get_data());
#endif

	const String cache_root = _get_cache_root();
	DirAccess::make_dir_recursive_absolute(cache_root);
	CefString(&settings.root_cache_path).FromString(cache_root.utf8().get_data());
	CefString(&settings.cache_path).FromString(cache_root.path_join("cache").utf8().get_data());
	CefString(&settings.log_file).FromString(cache_root.path_join("cef.log").utf8().get_data());
	settings.log_severity = OS::get_singleton()->is_stdout_verbose() ? LOGSEVERITY_INFO : LOGSEVERITY_WARNING;

	const int devtools_port = GodotCefSettings::get_remote_devtools_port();
#if defined(DEBUG_ENABLED)
	const bool allow_devtools = true;
#else
	const bool allow_devtools = Engine::get_singleton()->is_editor_hint();
#endif
	if (devtools_port > 0 && allow_devtools) {
		settings.remote_debugging_port = devtools_port;
	}
	const String user_agent = GodotCefSettings::get_user_agent();
	if (!user_agent.is_empty()) {
		CefString(&settings.user_agent).FromString(user_agent.utf8().get_data());
	}

	CefRefPtr<GodotCefApp> app = new GodotCefApp(GodotCefApp::make_config_from_settings());
	if (!CefInitialize(main_args, settings, app, nullptr)) {
		_fail(vformat("CefInitialize failed (exit code %d). See %s.", CefGetExitCode(), cache_root.path_join("cef.log")));
		_unload_library();
		return ERR_CANT_CREATE;
	}

	state = STATE_INITIALIZED;
	last_error = String();
	print_verbose(vformat("Godot CEF: initialized (runtime: %s).", paths.root_dir));
	ensure_frame_hook();
	return OK;
}

bool GodotCefRuntime::is_initialized() {
	return state == STATE_INITIALIZED;
}

GodotCefRuntime::State GodotCefRuntime::get_state() {
	return state;
}

String GodotCefRuntime::get_last_error() {
	return last_error;
}

void GodotCefRuntime::register_texture(CefTexture2D *p_texture) {
	textures.push_back(p_texture->get_instance_id());
	if (Thread::is_main_thread()) {
		ensure_frame_hook();
	}
}

void GodotCefRuntime::unregister_texture(CefTexture2D *p_texture) {
	textures.erase(p_texture->get_instance_id());
}

void GodotCefRuntime::ensure_frame_hook() {
	if (state == STATE_SHUT_DOWN || !Thread::is_main_thread()) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (!tree || tree->get_instance_id() == hooked_tree) {
		return;
	}
	// process_frame keeps firing in the editor's low-processor mode, unlike frame_pre_draw.
	tree->connect(SNAME("process_frame"), callable_mp_static(&GodotCefRuntime::process_frame));
	hooked_tree = tree->get_instance_id();
}

void GodotCefRuntime::schedule_message_pump_work(int64_t p_delay_ms) {
	const uint64_t due = OS::get_singleton()->get_ticks_usec() + uint64_t(MAX<int64_t>(p_delay_ms, 0)) * 1000;
	uint64_t current = pump_due_usec.load();
	while (due < current && !pump_due_usec.compare_exchange_weak(current, due)) {
	}
}

void GodotCefRuntime::notify_browser_opened() {
	open_browsers++;
}

void GodotCefRuntime::notify_browser_closed() {
	open_browsers = MAX(0, open_browsers - 1);
}

int GodotCefRuntime::get_open_browser_count() {
	return open_browsers;
}

void GodotCefRuntime::process_frame() {
	// Textures create browsers lazily, which may initialize CEF.
	const LocalVector<ObjectID> ids(textures);
	for (const ObjectID &id : ids) {
		CefTexture2D *texture = ObjectDB::get_instance<CefTexture2D>(id);
		if (!texture) {
			continue;
		}
		if (texture->get_reference_count() > 0) {
			Ref<CefTexture2D> keep = texture;
			texture->_process_frame();
		} else {
			texture->_process_frame();
		}
	}

	if (state != STATE_INITIALIZED) {
		return;
	}
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	if (pump_due_usec.load() <= now || now - last_pump_usec >= PUMP_FALLBACK_INTERVAL_USEC) {
		_pump();
	}
}

void GodotCefRuntime::shutdown() {
	if (state != STATE_INITIALIZED) {
		if (state == STATE_UNINITIALIZED || state == STATE_FAILED) {
			state = STATE_SHUT_DOWN;
		}
		_unload_library();
		return;
	}

	SceneTree *tree = ObjectDB::get_instance<SceneTree>(hooked_tree);
	if (tree) {
		tree->disconnect(SNAME("process_frame"), callable_mp_static(&GodotCefRuntime::process_frame));
	}
	hooked_tree = ObjectID();

	const LocalVector<ObjectID> ids(textures);
	for (const ObjectID &id : ids) {
		CefTexture2D *texture = ObjectDB::get_instance<CefTexture2D>(id);
		if (texture) {
			texture->_shutdown_for_exit();
		}
	}

	// Browsers close asynchronously; CefShutdown must not run while any is still open.
	const uint64_t start = OS::get_singleton()->get_ticks_usec();
	while (open_browsers > 0 && OS::get_singleton()->get_ticks_usec() - start < SHUTDOWN_TIMEOUT_USEC) {
		CefDoMessageLoopWork();
		OS::get_singleton()->delay_usec(1000);
	}
	if (open_browsers > 0) {
		WARN_PRINT(vformat("Godot CEF: %d browser(s) did not close before shutdown.", open_browsers));
	}
	for (int i = 0; i < 10; i++) {
		CefDoMessageLoopWork();
	}

	CefShutdown();
	state = STATE_SHUT_DOWN;
	_unload_library();
}
