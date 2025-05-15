#include "register_types.h"

#include "multi_godot.h"

#include "core/object/class_db.h"
#include "editor/plugins/editor_plugin.h"

void initialize_multi_godot_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

    GDREGISTER_CLASS(MultiGodot);
    EditorPlugins::add_by_type<MultiGodotPlugin>();
}

void uninitialize_multi_godot_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}