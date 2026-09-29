extends Control

@onready var menu: CefTexture = $Split/Menu
@onready var status: Label = $Status


func _ready() -> void:
	menu.ipc_message.connect(_on_menu_message)
	menu.ipc_data_message.connect(_on_menu_data)
	menu.load_finished.connect(_on_loaded)
	status.text = "Loading res://ui/ ..."


func _on_loaded(url: String, http_status_code: int) -> void:
	status.text = "Loaded %s (%d). Click the HTML menu or the 3D screen." % [url, http_status_code]


func _on_menu_message(message: String) -> void:
	status.text = "IPC from menu: %s" % message
	menu.send_ipc_message("ack:" + message)


func _on_menu_data(data: Variant) -> void:
	status.text = "Typed IPC from menu: %s" % str(data)
	menu.send_ipc_data({ "ok": true, "echo": data })


func _process(_delta: float) -> void:
	$Split/World/SubViewport/Screen.rotate_y(0.25 * _delta)
