#ifndef MULTI_GODOT_H
#define MULTI_GODOT_H

#include "core/object/ref_counted.h"

class MultiGodot : public RefCounted {
    GDCLASS(MultiGodot, RefCounted);

    protected:
        const int PACKET_READ_LIMIT = 32;
        const int MAX_MEMBERS = 4; // Possibly increase this in the future if there is a need.

        Steam *steam;
        int lobby_id = 0;
        Vector<String> lobby_members;

        static void _bind_methods();
        void _notification(int what);
        void _editor_ready();
        void _editor_process();
        void _create_lobby();
        void _join_lobby(int this_lobby_id);
        void _get_lobby_members();
        void _make_p2p_handshake();
        void _on_lobby_created(int connect, int this_lobby_id);
        void _on_lobby_match_list(Array these_lobbies);
        void _on_lobby_joined(int this_lobby_id, int _permissions, bool _locked, int response);
    
    public:
        MultiGodot();
};

#endif // MULTI_GODOT_H