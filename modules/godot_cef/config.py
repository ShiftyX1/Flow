def can_build(env, platform):
    return platform == "macos"


def get_opts(platform):
    from SCons.Variables import BoolVariable, PathVariable

    return [
        PathVariable(
            "godot_cef_runtime_path",
            "Path to a prepared Godot CEF macOS universal runtime bundle",
            "",
            PathVariable.PathAccept,
        ),
        BoolVariable(
            "godot_cef_require_runtime",
            "Fail the build if godot_cef_runtime_path is empty or invalid",
            False,
        ),
    ]


def configure(env):
    pass


def is_enabled():
    return False


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
