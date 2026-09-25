def can_build(env, platform):
    env.module_add_dependencies("moba_map", ["navigation_3d"])
    return not env["disable_3d"] and not env["disable_navigation_3d"]


def configure(env):
    pass


def get_doc_classes():
    return ["MobaMap3D", "MobaMapData", "MobaMapPalette", "MobaMapMarker3D", "MobaMapPreview3D"]


def get_doc_path():
    return "doc_classes"
