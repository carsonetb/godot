#include "register_types.h"

#include "core/object/class_db.h"
#include "multi_godot.h"

void initialize_multi_godot_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    ClassDB::register_class<MultiGodot>();
}

void uninitialize_multi_godot_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}