#include "multi_godot.h"

#include "core/variant/variant_utility.h"
#include "modules/godotsteam/godotsteam.h"
#include "register_types.h"
#include "scene/main/node.h"
#include "scene/gui/button.h"

MultiGodot::MultiGodot() {}

void MultiGodot::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_on_lobby_created", "connect", "this_lobby_id"), &MultiGodot::_on_lobby_created);
    ClassDB::bind_method(D_METHOD("_on_lobby_match_list", "these_lobbies"), &MultiGodot::_on_lobby_match_list);
    ClassDB::bind_method(D_METHOD("_on_lobby_joined", "this_lobby_id", "_permissions", "_locked", "response"), &MultiGodot::_on_lobby_joined);
    ClassDB::bind_method(D_METHOD("_on_lobby_chat_update", "this_lobby_id", "change_id", "making_change_id", "chat_state"), &MultiGodot::_on_lobby_chat_update);
    ClassDB::bind_method(D_METHOD("_on_p2p_session_request", "remote_id"), &MultiGodot::_on_p2p_session_request);
    ClassDB::bind_method(D_METHOD("_on_p2p_session_connect_fail", "steam_id", "session_error"), &MultiGodot::_on_p2p_session_connect_fail);

    // Setters & Getters
    ClassDB::bind_method(D_METHOD("set_remote_mouse_positions", "var"), &MultiGodot::set_remote_mouse_positions);
    ClassDB::bind_method(D_METHOD("get_remote_mouse_positions"), &MultiGodot::get_remote_mouse_positions);

    ClassDB::add_property("MultiGodot", PropertyInfo(Variant::Type::ARRAY, "remote_mouse_positions"), "set_remote_mouse_positions", "get_remote_mouse_positions");

}

void MultiGodot::_notification(int what) {
    if (what == NOTIFICATION_READY) {
        _ready();
    }
    if (what == NOTIFICATION_INTERNAL_PROCESS) {
        _process();
    }
    if (what == NOTIFICATION_DRAW) {
        _draw();
    }
}

void MultiGodot::_ready() {
    set_process_internal(true);
    steam = Steam::get_singleton();
    editor_node_singleton = EditorNode::get_singleton();

    if (steam) {
        ClassDB::register_class<MultiGodot>();
    } else {
        print_error("Steam module not found! MultiGodot will not be registered.");
        return;
    }

    Dictionary initialize_response = steam->steamInitEx(404790, true);
    if ((int)initialize_response["status"] > STEAM_API_INIT_RESULT_OK) {
        print_error(String("Steam did not initialize correctly. Response: "));
        print_line(initialize_response);
        return;
    }

    if (VERBOSE_DEBUG) {
        print_line("Successfully initialized MultiGodot module.");
    }

    this_project_name = ProjectSettings::get_singleton()->get("application/config/name");

    if (VERBOSE_DEBUG) {
        print_line("Project name: " + this_project_name);
    }

    steam->connect("lobby_created", Callable(this, "_on_lobby_created"));
    steam->connect("lobby_match_list", Callable(this, "_on_lobby_match_list"));
    steam->connect("lobby_joined", Callable(this, "_on_lobby_joined"));
    steam->connect("lobby_chat_update", Callable(this, "_on_lobby_chat_up"));
    steam->connect("p2p_session_request", Callable(this, "_on_p2p_session_request"));
    steam->connect("p2p_session_connect_fail", Callable(this, "_on_p2p_session_connect_fail"));
    steam->addRequestLobbyListDistanceFilter(LOBBY_DISTANCE_FILTER_WORLDWIDE);
    steam->requestLobbyList();
}

void MultiGodot::_process() {
    if (lobby_id > 0) {
        _read_all_p2p_packets(0);
    }

    _sync_var(this, "remote_mouse_positions");

    queue_redraw();
}

void MultiGodot::_draw() {
    for (int i = 0; i < remote_mouse_positions.size(); i++) {
        Vector2 pos = remote_mouse_positions[i];
        draw_circle(pos, 20, Color(1, 1, 1));
    }
}

// METHODS

void MultiGodot::_create_lobby() {
    if (lobby_id == 0) {
        is_lobby_owner = true;
        steam->createLobby(LOBBY_TYPE_PUBLIC, MAX_MEMBERS);
    }
}

void MultiGodot::_join_lobby(int this_lobby_id) {
    if (VERBOSE_DEBUG) {
        print_line(String("Attempting to join lobby ") + this_lobby_id);
    }

    lobby_members.clear();

    steam->joinLobby(this_lobby_id);
}

void MultiGodot::_get_lobby_members() {
    lobby_members.clear();

    int num_members = steam->getNumLobbyMembers(lobby_id);

    for (int this_member = 0; this_member < num_members; this_member++) {
        int member_steam_id = steam->getLobbyMemberByIndex(lobby_id, this_member);
        String member_steam_name = steam->getFriendPersonaName(member_steam_id);

        auto data_hashmap = HashMap<String, Variant>(2);
        data_hashmap["steam_id"] = member_steam_id;
        data_hashmap["steam_name"] = member_steam_name;

        lobby_members.append(data_hashmap);
    }
}

void MultiGodot::_make_p2p_handshake() {
    if (VERBOSE_DEBUG) {
        print_line("Sending P2P handshake to the lobby");
    }

    auto send_dictionary = Dictionary({
        KeyValue<Variant, Variant>("message", "handshake"), 
    });
    _send_p2p_packet(0, send_dictionary);
}

void MultiGodot::_send_p2p_packet(int this_target, Dictionary packet_data, P2PSend custom_send_type, int custom_channel) {
    PackedByteArray this_data = VariantUtilityFunctions::var_to_bytes(packet_data);

    // Sending to everyone, also don't send if you're the only one in the lobby.
    if (this_target == 0) {
        if (lobby_members.size() > 1) {
            for (int i = 0; i < lobby_members.size(); i++) {
                HashMap<String, Variant> this_member = lobby_members[i];
                if ((uint64_t)this_member.get("steam_id") != steam_id) {
                    steam->sendP2PPacket(this_member["steam_id"], this_data, custom_send_type, custom_channel);
                }
            }
        }
    }
    else {
        steam->sendP2PPacket(this_target, this_data, custom_send_type, custom_channel);
    }
}

void MultiGodot::_send_p2p_packet(int this_target, Dictionary packet_data) {
    _send_p2p_packet(this_target, packet_data, SEND_TYPE, DEFAULT_CHANNEL);
}

void MultiGodot::_read_all_p2p_packets(int read_count) {
    if (read_count >= PACKET_READ_LIMIT) {
        return;
    }

    if (steam->getAvailableP2PPacketSize(0) > 0) {
        _read_p2p_packet();
        _read_all_p2p_packets(read_count + 1);
    }
}

void MultiGodot::_read_p2p_packet() {
    int packet_size = steam->getAvailableP2PPacketSize(0);

    if (packet_size > 0) {
        Dictionary this_packet = steam->readP2PPacket(packet_size, 0);

        if (this_packet.is_empty()) {
            print_error("WARNING: Read an empty packet with non-zero size!");
            return;
        }

        int packet_sender = this_packet["remote_steam_id"];
        PackedByteArray packet_code = this_packet["data"];
        Dictionary readable_data = VariantUtilityFunctions::bytes_to_var(packet_code);

        // ALWAYS have message value. 
        String message = readable_data["message"];
        if (message == "handshake") {
            auto response = Dictionary({
                KeyValue<Variant, Variant>("message", "handshake_received"),
            });
            _send_p2p_packet(packet_sender, response);
        }
        if (message == "handshake_received") {
            handshake_completed_with.append(packet_sender);
        }
        if (message == "sync_var") {
            NodePath path = readable_data["path"];
            StringName property = readable_data["property"];
            Variant value = readable_data["value"];
            Node *node = editor_node_singleton->get_node_or_null(path);
            if (!node) {
                print_error("Remote set on null node at path" + String(path));
            }
            node->set(property, value);
        }
    }
}

void MultiGodot::_leave_lobby() {
    if (lobby_id != 0) {
        steam->leaveLobby(lobby_id);
        is_lobby_owner = false;
        lobby_id = 0;

        for (int i = 0; i < lobby_members.size(); i++) {
            HashMap<String, Variant> this_member = lobby_members[i];
            Variant this_user = this_member["steam_id"];
            if (this_user != Variant(steam_id)) {
                steam->closeP2PSessionWithUser(this_user);
            }
        }

        lobby_members.clear();
    }
}

void MultiGodot::_sync_var(Node *node, StringName property) {
    NodePath path = editor_node_singleton->get_path_to(node);
    Variant value = editor_node_singleton->get_node(path)->get(property);
    auto send_dictionary = Dictionary({
        KeyValue<Variant, Variant>("message", "sync_var"),
        KeyValue<Variant, Variant>("path", path),
        KeyValue<Variant, Variant>("property", property),
        KeyValue<Variant, Variant>("value", value),
    });
    _send_p2p_packet(0, send_dictionary);
}

// SIGNALS

void MultiGodot::_on_lobby_created(int connect, int this_lobby_id) {
    if (connect == 1) {
        lobby_id = this_lobby_id;
        if (VERBOSE_DEBUG) {
            print_line("Created a lobby");
        }

        steam->setLobbyJoinable(lobby_id, true);
        steam->setLobbyData(lobby_id, "name", this_project_name);
        steam->setLobbyData(lobby_id, "mode", "MultiGodotProject");
        steam->allowP2PPacketRelay(true);
    }
}

void MultiGodot::_on_lobby_match_list(Array these_lobbies) {
    if (VERBOSE_DEBUG) {
        print_line("Match lobby list:");
        print_line(these_lobbies);
    }
    for (int i = 0; i < these_lobbies.size(); i++) {
        uint64_t this_lobby = these_lobbies[i];
        
        String lobby_name = steam->getLobbyData(this_lobby, "name");
        String lobby_mode = steam->getLobbyData(this_lobby, "mode");
        // int lobby_num_members = steam->getNumLobbyMembers(this_lobby);

        if (lobby_name == this_project_name && lobby_mode == "MultiGodotProject") {
            _join_lobby(this_lobby);
            return;
        }
    }
    if (VERBOSE_DEBUG) {
        print_line("No lobbies found for this project, creating one.");
    }
    _create_lobby();
}

void MultiGodot::_on_lobby_joined(int this_lobby_id, int _permissions, bool _locked, int response) {
    if (response == CHAT_ROOM_ENTER_RESPONSE_SUCCESS) {
        lobby_id = this_lobby_id;

        _get_lobby_members();
        if (!is_lobby_owner) {
            _make_p2p_handshake();
        }
    }
    else {
        String fail_reason;

        switch (response) {
            case CHAT_ROOM_ENTER_RESPONSE_DOESNT_EXIST: fail_reason = "This lobby no longer exists."; break;
            case CHAT_ROOM_ENTER_RESPONSE_NOT_ALLOWED: fail_reason = "You don't have permission to join this lobby."; break;
            case CHAT_ROOM_ENTER_RESPONSE_FULL: fail_reason = "The lobby is now full."; break;
            case CHAT_ROOM_ENTER_RESPONSE_ERROR: fail_reason = "Uh... something unexpected happened!"; break;
            case CHAT_ROOM_ENTER_RESPONSE_BANNED: fail_reason = "You are banned from this lobby."; break;
            case CHAT_ROOM_ENTER_RESPONSE_LIMITED: fail_reason = "You cannot join due to having a limited account."; break;
            case CHAT_ROOM_ENTER_RESPONSE_CLAN_DISABLED: fail_reason = "This lobby is locked or disabled."; break;
            case CHAT_ROOM_ENTER_RESPONSE_COMMUNITY_BAN: fail_reason = "This lobby is community locked."; break;
            case CHAT_ROOM_ENTER_RESPONSE_MEMBER_BLOCKED_YOU: fail_reason = "A user in the lobby has blocked you from joining."; break;
            case CHAT_ROOM_ENTER_RESPONSE_YOU_BLOCKED_MEMBER: fail_reason = "A user you have blocked is in the lobby."; break;
        }

        print_error("Failed to join lobby associated with this project. Error: " + fail_reason);
    }
}

void MultiGodot::_on_lobby_chat_update(int this_lobby_id, int change_id, int making_change_id, int chat_state) {
    String changer_name = steam->getFriendPersonaName(change_id);

    if (VERBOSE_DEBUG) {
        switch (chat_state) {
            case CHAT_MEMBER_STATE_CHANGE_ENTERED:
                print_line(changer_name + " has joined the lobby."); break;
            case CHAT_MEMBER_STATE_CHANGE_LEFT:
                print_line(changer_name + " has left"); break;
        }
    }

    _get_lobby_members();
}

void MultiGodot::_on_p2p_session_request(int remote_id) {
    String this_requester = steam->getFriendPersonaName(remote_id);
    if (VERBOSE_DEBUG) {
        print_line(this_requester + " is requesting a P2P session.");
    }

    steam->acceptP2PSessionWithUser(remote_id);

    if (!is_lobby_owner) {
        _make_p2p_handshake();
    }
}

void MultiGodot::_on_p2p_session_connect_fail(int this_steam_id, int session_error) {
    switch (session_error) {
        case 0: print_line("WARNING: Session failure: no error given (task failed successfully!)"); break;
        case 1: print_line("WARNING: Session failure: target user not running the same game"); break;
        case 2: print_line("WARNING: Session failure: local user doesn't own app / game"); break;
        case 3: print_line("WARNING: Session failure: target user isn't connected to Steam"); break;
        case 4: print_line("WARNING: Session failure: connection timed out"); break;
        case 5: print_line("WARNING: Session failure: unused"); break;
        default:print_line("WARNING: Session failure: unknown error"); break;
    }
}

// PLUGIN

MultiGodotPlugin::MultiGodotPlugin() {
    multi_godot = memnew(MultiGodot);
    EditorNode *node = EditorNode::get_singleton();
    node->add_child(multi_godot);
    set_owner(node);
}