"""Functions used to generate source files and the CEF runtime bundle during build time."""

import os
import shutil

MACOS_RUNTIME_APP = "Godot CEF.app"
MACOS_HELPER_NAME = "Godot CEF Helper"
MACOS_HELPER_VARIANTS = ["", " (GPU)", " (Renderer)", " (Plugin)", " (Alerts)"]
MACOS_FRAMEWORK = "Chromium Embedded Framework.framework"
WINDOWS_HELPER = "godot_cef_helper.exe"

_PLATFORM_SUFFIXES = {
    "macos": ("_mac",),
    "windows": ("_win",),
}
_ALL_SUFFIXES = ("_mac", "_win", "_linux")


def get_wrapper_sources(sdk_path, platform):
    """Returns the libcef_dll_wrapper sources for the given platform, like libcef_dll/CMakeLists.txt does."""
    wrapper_root = os.path.join(sdk_path, "libcef_dll")
    wanted = _PLATFORM_SUFFIXES.get(platform, ())
    sources = []
    for root, _dirs, files in os.walk(wrapper_root):
        for name in sorted(files):
            stem, ext = os.path.splitext(name)
            if ext not in (".cc", ".mm"):
                continue
            if ext == ".mm" and platform != "macos":
                continue
            suffix = next((s for s in _ALL_SUFFIXES if stem.endswith(s)), None)
            if suffix is not None and suffix not in wanted:
                continue
            if name == "libcef_dll_dylib.cc" and platform != "macos":
                continue
            sources.append(os.path.join(root, name))
    return sorted(sources)


def _write_plist(path, values):
    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">',
        '<plist version="1.0">',
        "<dict>",
    ]
    for key, value in values.items():
        lines.append(f"\t<key>{key}</key>")
        if isinstance(value, bool):
            lines.append("\t<true/>" if value else "\t<false/>")
        elif isinstance(value, dict):
            lines.append("\t<dict>")
            for sub_key, sub_value in value.items():
                lines.append(f"\t\t<key>{sub_key}</key>")
                lines.append(f"\t\t<string>{sub_value}</string>")
            lines.append("\t</dict>")
        else:
            lines.append(f"\t<string>{value}</string>")
    lines += ["</dict>", "</plist>", ""]
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def _macos_app_plist(exec_name, bundle_id, is_helper):
    values = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": exec_name,
        "CFBundleExecutable": exec_name,
        "CFBundleIdentifier": bundle_id,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": exec_name,
        "CFBundlePackageType": "APPL",
        "CFBundleSignature": "????",
        "CFBundleVersion": "1.0.0",
        "CFBundleShortVersionString": "1.0",
        "LSEnvironment": {"MallocNanoZone": "0"},
        "LSFileQuarantineEnabled": True,
        "LSMinimumSystemVersion": "11.0",
        "NSSupportsAutomaticGraphicsSwitching": True,
        "NSBluetoothAlwaysUsageDescription": exec_name,
        "NSCameraUsageDescription": exec_name,
        "NSMicrophoneUsageDescription": exec_name,
        "NSWebBrowserPublicKeyCredentialUsageDescription": exec_name,
    }
    if is_helper:
        values["LSUIElement"] = "1"
    return values


def _create_macos_app(parent_dir, exec_name, bundle_id, binary, is_helper):
    app_dir = os.path.join(parent_dir, exec_name + ".app")
    macos_dir = os.path.join(app_dir, "Contents", "MacOS")
    os.makedirs(macos_dir, exist_ok=True)
    os.makedirs(os.path.join(app_dir, "Contents", "Resources"), exist_ok=True)
    _write_plist(os.path.join(app_dir, "Contents", "Info.plist"), _macos_app_plist(exec_name, bundle_id, is_helper))
    shutil.copy2(binary, os.path.join(macos_dir, exec_name))
    return app_dir


def _bundle_macos(runtime_dir, helper_binary, sdk_path):
    app_path = os.path.join(runtime_dir, MACOS_RUNTIME_APP)
    if os.path.exists(app_path):
        shutil.rmtree(app_path)

    _create_macos_app(runtime_dir, "Godot CEF", "org.godotengine.godot-cef", helper_binary, False)
    frameworks_dir = os.path.join(app_path, "Contents", "Frameworks")
    os.makedirs(frameworks_dir, exist_ok=True)
    shutil.copytree(
        os.path.join(sdk_path, "Release", MACOS_FRAMEWORK),
        os.path.join(frameworks_dir, MACOS_FRAMEWORK),
        symlinks=True,
    )
    for variant in MACOS_HELPER_VARIANTS:
        name = MACOS_HELPER_NAME + variant
        suffix = variant.strip(" ()").lower()
        bundle_id = "org.godotengine.godot-cef.helper" + ("." + suffix if suffix else "")
        _create_macos_app(frameworks_dir, name, bundle_id, helper_binary, True)


def _copy_tree_contents(src_dir, dst_dir, skip_ext=()):
    for name in os.listdir(src_dir):
        src = os.path.join(src_dir, name)
        dst = os.path.join(dst_dir, name)
        if os.path.isdir(src):
            if os.path.exists(dst):
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
        elif os.path.splitext(name)[1].lower() not in skip_ext:
            shutil.copy2(src, dst)


def _bundle_windows(runtime_dir, helper_binary, sdk_path):
    _copy_tree_contents(os.path.join(sdk_path, "Release"), runtime_dir, skip_ext=(".lib", ".pdb"))
    _copy_tree_contents(os.path.join(sdk_path, "Resources"), runtime_dir)
    shutil.copy2(helper_binary, os.path.join(runtime_dir, WINDOWS_HELPER))


def make_runtime_bundle(target, source, env):
    stamp = str(target[0])
    runtime_dir = os.path.dirname(stamp)
    helper_binary = str(source[0])
    sdk_path = env["GODOT_CEF_SDK_PATH"]
    os.makedirs(runtime_dir, exist_ok=True)

    if env["GODOT_CEF_PLATFORM"] == "macos":
        _bundle_macos(runtime_dir, helper_binary, sdk_path)
    else:
        _bundle_windows(runtime_dir, helper_binary, sdk_path)

    with open(os.path.join(sdk_path, "include", "cef_version.h"), encoding="utf-8") as f:
        version = next((line.split('"')[1] for line in f if line.startswith("#define CEF_VERSION ")), "unknown")
    with open(stamp, "w", encoding="utf-8") as f:
        f.write(version + "\n")
