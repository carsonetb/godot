#include "multi_godot.h"

#include "register_types.h"
#include "modules/godotsteam/godotsteam.h"
#include "scene/main/node.h"

MultiGodot::MultiGodot() {

}

void MultiGodot::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_notification", "what"), &MultiGodot::_notification);
}

void MultiGodot::_notification(int what) {
    if (what == Node::NOTIFICATION_READY) {
        _editor_ready();
    }
    if (what == Node::NOTIFICATION_PROCESS) {
        _editor_process();
    }
}

void MultiGodot::_editor_ready() {
    steam = Steam::get_singleton();
    if (steam) {
        ClassDB::register_class<MultiGodot>();
    } else {
        ERR_PRINT("Steam module not found! MultiGodot will not be registered.");
        return;
    }

    Dictionary initialize_response = steam->steamInitEx(404790, true);
    if ((int)initialize_response["status"] > STEAM_API_INIT_RESULT_OK) {
        print_error(String("Steam did not initialize correctly. Response: "));
        print_line(initialize_response);
        return;
    }

    print_line("Successfully initialized MultiGodot module.");

    steam->connect("lobby_created", Callable(this, "_on_lobby_created"));
    steam->connect("lobby_match_list", Callable(this, "_on_lobby_match_list"));
    steam->connect("lobby_joined", Callable(this, "_on_lobby_joined"));

    steam->addRequestLobbyListDistanceFilter(LOBBY_DISTANCE_FILTER_WORLDWIDE);
    steam->requestLobbyList();

    _create_lobby();
}

void MultiGodot::_editor_process() {

}

// METHODS

void MultiGodot::_create_lobby() {
    if (lobby_id == 0) {
        steam->createLobby(LOBBY_TYPE_PUBLIC, MAX_MEMBERS);
    }
}

void MultiGodot::_join_lobby(int this_lobby_id) {
    print_line(String("Attempting to join lobby ") + this_lobby_id);

    lobby_members.clear();

    steam->joinLobby(this_lobby_id);
}

// SIGNALS

void MultiGodot::_on_lobby_created(int connect, int this_lobby_id) {
    if (connect == 1) {
        lobby_id = this_lobby_id;
        print_line("Created a lobby: " + lobby_id);

        steam->setLobbyJoinable(lobby_id, true);
        steam->setLobbyData(lobby_id, "name", ProjectSettings::get_singleton()->get("application/name"));
        steam->setLobbyData(lobby_id, "mode", "MultiGodotProject");
        steam->allowP2PPacketRelay(true);
    }
}

void MultiGodot::_on_lobby_match_list(Array these_lobbies) {
    for (int i = 0; i < these_lobbies.size(); i++) {
        Variant this_lobby = these_lobbies[i];
        
        String lobby_name = steam->getLobbyData(this_lobby, "name");
        String lobby_mode = steam->getLobbyData(this_lobby, "mode");
        int lobby_num_members = steam->getNumLobbyMembers(this_lobby);
    }
}

void MultiGodot::_on_lobby_joined(int this_lobby_id, int _permissions, bool _locked, int response) {
    if (response == CHAT_ROOM_ENTER_RESPONSE_SUCCESS) {
        lobby_id = this_lobby_id;

        _get_lobby_members();
        _make_p2p_handshake();
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
    }
}

void MultiGodot::_get_lobby_members() {

}

void MultiGodot::_make_p2p_handshake() {

}