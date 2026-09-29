import os

CEF_PLATFORMS = {
    ("macos", "arm64"): "macosarm64",
    ("macos", "x86_64"): "macosx64",
    ("windows", "x86_64"): "windows64",
}


def _default_deps_folder():
    local_app_data = os.getenv("LOCALAPPDATA")
    if local_app_data:
        return os.path.join(local_app_data, "Godot", "build_deps", "cef")
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "bin", "build_deps", "cef")


def get_cef_sdk_path(env):
    cef_platform = CEF_PLATFORMS.get((env["platform"], env["arch"]))
    if cef_platform is None:
        return None
    sdk_path = env.get("cef_sdk_path", "")
    if not sdk_path:
        sdk_path = os.path.join(_default_deps_folder(), cef_platform)
    sdk_path = os.path.abspath(sdk_path)
    if not os.path.isfile(os.path.join(sdk_path, "include", "cef_version.h")):
        return None
    return sdk_path


def can_build(env, platform):
    if platform not in ("macos", "windows"):
        return False
    if (platform, env["arch"]) not in CEF_PLATFORMS:
        print(f"godot_cef: CEF is not available for {platform}.{env['arch']}, module disabled.")
        return False
    if platform == "windows" and not env.msvc:
        print("godot_cef: the CEF binary distribution requires MSVC on Windows, module disabled.")
        return False
    if get_cef_sdk_path(env) is None:
        print(
            "godot_cef: CEF SDK not found, module disabled. Run "
            "`python modules/godot_cef/tools/fetch_cef_sdk.py` or pass `cef_sdk_path=<path>`."
        )
        return False
    return True


def get_opts(platform):
    from SCons.Variables import BoolVariable, PathVariable

    return [
        PathVariable(
            "cef_sdk_path",
            "Path to the CEF binary distribution (defaults to bin/build_deps/cef/<platform>)",
            "",
            PathVariable.PathAccept,
        ),
        BoolVariable(
            "godot_cef_bundle_runtime",
            "Assemble the CEF runtime (framework, helpers, resources) next to the built binary",
            True,
        ),
    ]


def configure(env):
    env["GODOT_CEF_SDK_PATH"] = get_cef_sdk_path(env)


def get_doc_classes():
    return [
        "CefTexture",
        "CefTexture2D",
        "CefIpcInspector",
        "DragDataInfo",
        "DragOperation",
        "DownloadRequestInfo",
        "DownloadUpdateInfo",
        "CookieInfo",
    ]


def get_doc_path():
    return "doc_classes"
