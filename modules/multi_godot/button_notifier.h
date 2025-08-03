#ifndef BUTTON_NOTIFIER_H
#define BUTTON_NOTIFIER_H

#include "scene/main/node.h"

class ButtonNotifier : public Node {
    GDCLASS(ButtonNotifier, Node);

    protected:
        HashMap<String, Variant> previous_values;

        static void _bind_methods();
        void _notification(int what);
        void _ready();
        void _process();
};

#endif // BUTTON_NOTIFIER_H