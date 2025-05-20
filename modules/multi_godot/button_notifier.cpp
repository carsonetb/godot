#include "button_notifier.h"

#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/plugins/script_editor_plugin.h"
#include "scene/gui/tab_container.h"

void ButtonNotifier::_bind_methods() {
    INIT_SIGNAL("main_screen_selected", Variant::INT);
    INIT_SIGNAL("script_tab_changed", Variant::STRING);
}

void ButtonNotifier::_notification(int what) {
    switch (what) {
        case NOTIFICATION_READY: _ready(); break;
        case NOTIFICATION_INTERNAL_PROCESS: _process(); break;
    };
}

void ButtonNotifier::_ready() {
    set_process_internal(true);

    INIT("main_screen_selected", EditorNode::get_editor_main_screen()->get_selected_index());
    if (ScriptEditor::get_singleton()->_get_current_script() != nullptr) {
        INIT("script_tab_changed", ScriptEditor::get_singleton()->_get_current_script()->get_path());
    }
}

void ButtonNotifier::_process() {
    DIFFPREV(EditorNode::get_editor_main_screen()->get_selected_index(), "main_screen_selected", int);
    if (ScriptEditor::get_singleton()->_get_current_script() != nullptr) {
        DIFFPREV(ScriptEditor::get_singleton()->_get_current_script()->get_path(), "script_tab_changed", String);
    }
}