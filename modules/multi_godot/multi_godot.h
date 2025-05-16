#ifndef MULTI_GODOT_H
#define MULTI_GODOT_H

#include "core/object/ref_counted.h"
#include "editor/editor_node.h"
#include "editor/plugins/editor_plugin.h"
#include "modules/godotsteam/godotsteam.h"
#include "scene/2d/node_2d.h"

class MultiGodot : public Node2D {
    GDCLASS(MultiGodot, Node2D);

    protected:
        static const int PACKET_READ_LIMIT = 32;
        static const int MAX_MEMBERS = 4; // Possibly increase this in the future if there is a need.
        static const bool VERBOSE_DEBUG = true;
        static const P2PSend SEND_TYPE = P2P_SEND_UNRELIABLE;
        static const int DEFAULT_CHANNEL = 0;

        Steam *steam;
        uint64_t lobby_id = 0;
        uint64_t steam_id = 0;
        bool is_lobby_owner = false;
        String this_project_name;
        Vector<HashMap<String, Variant>> lobby_members;
        Vector<uint64_t> handshake_completed_with;
        EditorNode *editor_node_singleton;
        bool printed = false;

        // Remote properties
        HashMap<uint64_t, Vector2> mouse_positions;

        // BUILTINS

        static void _bind_methods();
        void _notification(int what);
        void _ready();
        void _process();
        void _draw();

        // METHODS
        void _create_lobby();
        void _join_lobby(uint64_t this_lobby_id);
        void _get_lobby_members();
        void _make_p2p_handshake();
        void _send_p2p_packet(uint64_t this_target, Dictionary packet_data, P2PSend custom_send_type, int custom_channel);
        void _send_p2p_packet(uint64_t this_target, Dictionary packet_data);
        void _read_all_p2p_packets(int read_count);
        void _read_p2p_packet();
        void _leave_lobby();
        void _sync_var(Node *node, StringName property);
        void _call_func(Node *node, String function_name, Array args);

        // REMOTE CALLABLES

        void _set_mouse_position(uint64_t sender, Vector2 pos) {mouse_positions.insert(sender, pos);}

        // SIGNALS

        void _on_lobby_created(int connect, uint64_t this_lobby_id);
        void _on_lobby_match_list(Array these_lobbies);
        void _on_lobby_joined(uint64_t this_lobby_id, int _permissions, bool _locked, int response);
        void _on_lobby_chat_update(uint64_t this_lobby_id, uint64_t change_id, uint64_t making_change_id, int chat_state);
        void _on_p2p_session_request(uint64_t remote_id);
        void _on_p2p_session_connect_fail(uint64_t this_steam_id, int session_error);
    
    public:
        MultiGodot();

        // SETTERS & GETTERS

};

class MultiGodotPlugin : public EditorPlugin {
    GDCLASS(MultiGodotPlugin, EditorPlugin);

    protected:
        MultiGodot *multi_godot;

    public:
        MultiGodotPlugin();
};

#endif // MULTI_GODOT_H