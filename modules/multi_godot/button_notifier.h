#ifndef BUTTON_NOTIFIER_H
#define BUTTON_NOTIFIER_H

#include "scene/main/node.h"

class ButtonNotifier : public Node {
    GDCLASS(ButtonNotifier, Node);

    // This probably defies every rule of C++ but I don't want to type.
    #define DIFFPREV(func, string, type) \
        type name = func; \
        if (name != (type)previous_values.get(string)) { \
            emit_signal(string, name); \
        } \
    // DIFFPREV

    #define INIT(string, func) \
        previous_values.insert(string, func);
    // INIT

    #define INIT_SIGNAL(string, variant_type) \
        ADD_SIGNAL(MethodInfo(string, PropertyInfo(variant_type, "param"))) 
    // INIT_SIGNAL

    protected:
        HashMap<String, Variant> previous_values;

        static void _bind_methods();
        void _notification(int what);
        void _ready();
        void _process();
};

#endif // BUTTON_NOTIFIER_H