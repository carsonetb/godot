#ifndef STEAM_INTEGRATION_H
#define STEAM_INTEGRATION_H

#include "editor/editor_node.h"
#include "modules/godotsteam/godotsteam.h"
#include "scene/main/node.h"

class SteamIntegration : public Node {
    protected:

        static const int PACKET_READ_LIMIT = 32;
        static const int MAX_MEMBERS = 4; // Possibly increase this in the future if there is a need.
        static const P2PSend SEND_TYPE = P2P_SEND_UNRELIABLE;
        static const int DEFAULT_CHANNEL = 0;

        Steam *steam;
        EditorNode *editor_node_singleton;

        String this_project_name; 

        // BUILTINS

        void _bind_methods();
        void _notification(int what);
        void _ready();
        void _process();

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

        // SIGNALS

        void _on_lobby_created(int connect, uint64_t this_lobby_id);
        void _on_lobby_match_list(Array these_lobbies);
        void _on_lobby_joined(uint64_t this_lobby_id, int _permissions, bool _locked, int response);
        void _on_lobby_chat_update(uint64_t this_lobby_id, uint64_t change_id, uint64_t making_change_id, int chat_state);
        void _on_p2p_session_request(uint64_t remote_id);
        void _on_p2p_session_connect_fail(uint64_t this_steam_id, int session_error);
    
    public:

        String platform;
        uint64_t lobby_id = 0;
        uint64_t steam_id = 0;
        String steam_name;
        bool is_lobby_owner = false;
        Vector<HashMap<String, Variant>> lobby_members;
        Vector<uint64_t> handshake_completed_with;

        void sync_var(Node *node, StringName property, uint64_t custom_target = 0);
        void call_func(Node *node, String function_name, Array args, uint64_t custom_target = 0);
};

#endif // STEAM_INTEGRATION_H