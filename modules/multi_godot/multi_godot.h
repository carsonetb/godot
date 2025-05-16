#ifndef MULTI_GODOT_H
#define MULTI_GODOT_H

#include "core/object/ref_counted.h"
#include "modules/godotsteam/godotsteam.h"
#include "editor/editor_node.h"
#include "editor/plugins/editor_plugin.h"

class MultiGodot : public Node {
    GDCLASS(MultiGodot, Node);

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
        Vector<int> handshake_completed_with;
        EditorNode *editor_node_singleton;

        static void _bind_methods();
        void _create_lobby();
        void _join_lobby(int this_lobby_id);
        void _notification(int what);
        void _ready();
        void _process();
        void _get_lobby_members();
        void _make_p2p_handshake();
        void _send_p2p_packet(int this_target, Dictionary packet_data, P2PSend custom_send_type, int custom_channel);
        void _send_p2p_packet(int this_target, Dictionary packet_data);
        void _read_all_p2p_packets(int read_count);
        void _read_p2p_packet();
        void _leave_lobby();
        void _on_lobby_created(int connect, int this_lobby_id);
        void _on_lobby_match_list(Array these_lobbies);
        void _on_lobby_joined(int this_lobby_id, int _permissions, bool _locked, int response);
        void _on_lobby_chat_update(int this_lobby_id, int change_id, int making_change_id, int chat_state);
        void _on_p2p_session_request(int remote_id);
        void _on_p2p_session_connect_fail(int this_steam_id, int session_error);
    
    public:
        MultiGodot();
};

class MultiGodotPlugin : public EditorPlugin {
    GDCLASS(MultiGodotPlugin, EditorPlugin);

    protected:
        MultiGodot *multi_godot;

    public:
        MultiGodotPlugin();
};

#endif // MULTI_GODOT_H