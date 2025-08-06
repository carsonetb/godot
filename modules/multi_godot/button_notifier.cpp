#include "button_notifier.h"

#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/plugins/script_editor_plugin.h"
#include "scene/gui/tab_container.h"

void ButtonNotifier::_bind_methods() {
    ADD_SIGNAL(MethodInfo("editor_tab_changed", PropertyInfo(Variant::INT, "new_tab_index")));
    ADD_SIGNAL(MethodInfo("current_script_path_changed", PropertyInfo(Variant::STRING, "new_path")));
}

void ButtonNotifier::_notification(int what) {
    switch (what) {
        case NOTIFICATION_READY: _ready(); break;
        case NOTIFICATION_INTERNAL_PROCESS: _process(); break;
    };
}

void ButtonNotifier::_ready() {
    set_process_internal(true);

    int tab = EditorNode::get_editor_main_screen()->get_selected_index();

    previous_values.insert("editor_tab", tab);
    previous_values.insert("current_script_path", "");
    call_deferred("emit_signal", "editor_tab_changed", tab);
    call_deferred("emit_signal", "current_script_path_changed", "");
}

void ButtonNotifier::_process() {
    int index = EditorNode::get_editor_main_screen()->get_selected_index();
    if (index != (int)previous_values.get("editor_tab")) {
        previous_values.insert("editor_tab", index);
        emit_signal("editor_tab_changed", index);
    }

    if (ScriptEditor::get_singleton()->_get_current_script() != nullptr) {
        String path = ScriptEditor::get_singleton()->_get_current_script()->get_path();
        if (path != (String)previous_values.get("current_script_path")) {
            previous_values.insert("current_script_path", path);
            emit_signal("current_script_path_changed", path);
        }
    }
}