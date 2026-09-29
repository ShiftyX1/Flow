/**************************************************************************/
/*  godot_cef_export_plugin.h                                             */
/**************************************************************************/

#pragma once

#include "editor/export/editor_export_plugin.h"
#include "editor/plugins/editor_plugin.h"

// Copies the CEF runtime (GodotCEF directory) next to exported macOS and Windows builds.
class GodotCefExportPlugin : public EditorExportPlugin {
	GDCLASS(GodotCefExportPlugin, EditorExportPlugin);

	static bool _is_supported_os(const String &p_os_name);
	String _find_runtime_dir(const String &p_os_name) const;

protected:
	virtual void _get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const override;
	virtual String _get_export_option_warning(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const override;
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;

public:
	virtual String get_name() const override { return "GodotCEF"; }
	virtual bool supports_platform(const Ref<EditorExportPlatform> &p_export_platform) const override;
};

class GodotCefEditorPlugin : public EditorPlugin {
	GDCLASS(GodotCefEditorPlugin, EditorPlugin);

	Ref<GodotCefExportPlugin> export_plugin;

protected:
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "GodotCEF"; }
};
