#include "multi_godot.h"

#include "core/io/dir_access.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/object/script_language.h"
#include "core/templates/pair.h"
#include "core/variant/variant_utility.h"
#include "editor/code_editor.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_main_screen.h"
#include "editor/plugins/script_editor_plugin.h"
#include "modules/godotsteam/godotsteam.h"
#include "register_types.h"
#include "scene/main/node.h"
#include "scene/gui/button.h"
#include "scene/gui/code_edit.h"

MultiGodot::MultiGodot() {}

void MultiGodot::_bind_methods() {

    // Signals

    ClassDB::bind_method(D_METHOD("_on_lobby_created", "connect", "this_lobby_id"), &MultiGodot::_on_lobby_created);
    ClassDB::bind_method(D_METHOD("_on_lobby_match_list", "these_lobbies"), &MultiGodot::_on_lobby_match_list);
    ClassDB::bind_method(D_METHOD("_on_lobby_joined", "this_lobby_id", "_permissions", "_locked", "response"), &MultiGodot::_on_lobby_joined);
    ClassDB::bind_method(D_METHOD("_on_lobby_chat_update", "this_lobby_id", "change_id", "making_change_id", "chat_state"), &MultiGodot::_on_lobby_chat_update);
    ClassDB::bind_method(D_METHOD("_on_p2p_session_request", "remote_id"), &MultiGodot::_on_p2p_session_request);
    ClassDB::bind_method(D_METHOD("_on_p2p_session_connect_fail", "steam_id", "session_error"), &MultiGodot::_on_p2p_session_connect_fail);

    // Remote Callables

    ClassDB::bind_method(D_METHOD("_set_mouse_position", "sender", "position"), &MultiGodot::_set_mouse_position);
    ClassDB::bind_method(D_METHOD("_set_user_data", "sender", "item", "data"), &MultiGodot::_set_user_data);
    ClassDB::bind_method(D_METHOD("_update_script_different", "path", "remote_code"), &MultiGodot::_update_script_different);
    ClassDB::bind_method(D_METHOD("_update_script_same", "line", "line_text", "newline", "indent_from_line", "indent_from_column"), &MultiGodot::_update_script_same);
    ClassDB::bind_method(D_METHOD("_update_script_same_skewed", "from", "contents"), &MultiGodot::_update_script_same_skewed);
    ClassDB::bind_method(D_METHOD("_compare_filesystem", "other_path_list", "host_id"), &MultiGodot::_compare_filesystem);
    ClassDB::bind_method(D_METHOD("_request_file_contents", "client_id"), &MultiGodot::_request_file_contents);
    ClassDB::bind_method(D_METHOD("_receive_file_contents", "path", "contents"), &MultiGodot::_receive_file_contents);
    ClassDB::bind_method(D_METHOD("_delete_file", "path"), &MultiGodot::_delete_file);
    ClassDB::bind_method(D_METHOD("_rename_file", "from", "to"), &MultiGodot::_rename_file);
    ClassDB::bind_method(D_METHOD("_sync_user_data", "user_id", "individual_data"), &MultiGodot::_sync_user_data);

    // Button signals

    ClassDB::bind_method(D_METHOD("_on_editor_tab_changed", "index"), &MultiGodot::_on_editor_tab_changed);
    ClassDB::bind_method(D_METHOD("_on_current_script_path_changed", "path"), &MultiGodot::_on_current_script_path_changed);

    // Setters & Getters

}

void MultiGodot::_notification(int what) {
    if (what == NOTIFICATION_READY) {
        _ready();
    }
    if (what == NOTIFICATION_INTERNAL_PROCESS) {
        _process();
    }
    if (what == NOTIFICATION_EXIT_TREE) {
        _exit_tree();
    }
    if (what == NOTIFICATION_DRAW) {
        _draw();
    }
}

void MultiGodot::_ready() {
    set_process_internal(true);

    steam = Steam::get_singleton();
    editor_node_singleton = EditorNode::get_singleton();
    script_editor = ScriptEditor::get_singleton();

    button_notifier = memnew(ButtonNotifier);
    add_child(button_notifier);

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

    steam_id = steam->getSteamID();
    user_data.insert(steam_id, HashMap<String, Variant>());

    this_project_name = ProjectSettings::get_singleton()->get("application/config/name");

    if (VERBOSE_DEBUG) {
        print_line("Project name: " + this_project_name);
    }

    filesystem_scanner.start(_threaded_filesystem_scanner, this);

    button_notifier->connect("editor_tab_changed", Callable(this, "_on_editor_tab_changed"));
    button_notifier->connect("current_script_path_changed", Callable(this, "_on_current_script_path_changed"));
    steam->connect("lobby_created", Callable(this, "_on_lobby_created"));
    steam->connect("lobby_match_list", Callable(this, "_on_lobby_match_list"));
    steam->connect("lobby_joined", Callable(this, "_on_lobby_joined"));
    steam->connect("lobby_chat_update", Callable(this, "_on_lobby_chat_up"));
    steam->connect("p2p_session_request", Callable(this, "_on_p2p_session_request"));
    steam->connect("p2p_session_connect_fail", Callable(this, "_on_p2p_session_connect_fail"));
    steam->addRequestLobbyListDistanceFilter(LOBBY_DISTANCE_FILTER_WORLDWIDE);
    steam->requestLobbyList();

    const Vector<String> initial_keys = {
        "script_current_line",
        "editor_tab_index",
        "current_script_path",
        "current_spectating_script",
    };

    const Vector<Variant> initial_values = {0, 0, "", ""};

    for (int i = 0; i < initial_keys.size(); i++) {
        _set_user_data(steam_id, initial_keys[i], initial_values[i]);
    }
}

void MultiGodot::_process() {
    steam->run_callbacks();

    if (lobby_id > 0) {
        _read_all_p2p_packets(0);
    }

    _call_func(this, "_set_mouse_position", {steam_id, Input::get_singleton()->get_mouse_position()});

    _sync_created_deleted_files();
    _sync_scripts();
    if (ENABLE_SAME_FILE_EDITS) {
        _sync_live_edits();
    }
    else {
        _sync_live_edits_skewed();
    }
    
    queue_redraw();
}

void MultiGodot::_exit_tree() {
    stop_filesystem_scanner = true;
    filesystem_scanner.wait_to_finish();
    steam->leaveLobby(lobby_id);
}

void MultiGodot::_draw() {
    for (int i = 0; i < handshake_completed_with.size(); i++) {
        uint64_t key = handshake_completed_with[i];
        if (!mouse_positions.has(key)) {
            continue;
        }
        Vector2 pos = mouse_positions.get(key);
        draw_circle(pos, 20, Color(1, 1, 1));
    }
}

// METHODS

void MultiGodot::_threaded_filesystem_scanner(void *p_userdata) {
    MultiGodot *this_object = (MultiGodot *)p_userdata;

    String project_path = ProjectSettings::get_singleton()->get_resource_path();
    Vector<String> old_file_list = _get_file_path_list(project_path);

    while (true) {
        this_object->mutex.lock();
        bool stop_filesystem_scanner = this_object->stop_filesystem_scanner;
        this_object->mutex.unlock();

        if (stop_filesystem_scanner) {
            return;
        }

        Vector<String> new_files;
        Vector<String> deleted_files;

        Vector<String> file_list = _get_file_path_list(project_path);

        // Check for new files.
        for (int i = 0; i < file_list.size(); i++) {
            String path = file_list[i];
            if (path.to_lower().ends_with("tmp") || path.to_lower().ends_with("ini")) {
                continue;
            }
            if (!old_file_list.has(path)) {
                new_files.append(path);
            }
        }

        // Check for old files.
        for (int i = 0; i < old_file_list.size(); i++) {
            String path = old_file_list[i];
            if (path.to_lower().ends_with("tmp") || path.to_lower().ends_with("ini")) {
                continue;
            }
            if (!file_list.has(path)) {
                deleted_files.append(path);
            }
        }

        old_file_list = file_list;

        this_object->mutex.lock();
        this_object->new_files.append_array(new_files);
        this_object->deleted_files.append_array(deleted_files);
        this_object->mutex.unlock();
    }
}

Dictionary MultiGodot::_hashmap_to_dictionary(HashMap<String, Variant> map) {
    Dictionary out;

    HashMap<String, Variant>::Iterator i = map.begin();
    while (i != map.end()) {
        KeyValue<String, Variant> key_value = i.operator*(); // No idea if this is correct...
        out.set(key_value.key, key_value.value);
        i = i.operator++();
    }

    return out;
}

HashMap<String, Variant> MultiGodot::_dictionary_to_hashmap(Dictionary dict) {
    HashMap<String, Variant> out;

    Array keys = dict.keys();
    for (int i = 0; i < keys.size(); i++) {
        out[keys[i]] = dict.get(keys[i], Variant());
    }

    return out;
}

Vector<String> MultiGodot::_get_file_path_list(String path, String localized_path) {
    Vector<String> files;

    Ref<DirAccess> dir = DirAccess::open(path);
    if (!dir.is_valid()) {
        return files;
    }

    dir->list_dir_begin();

    String file_name;
    while (true) {
        file_name = dir->get_next();
        if (file_name == "") break;
        if (file_name == "." || file_name == ".." || file_name == ".godot") continue;

        String file_path = path + "/" + file_name;
        String localized_file_path = localized_path + "/" + file_name;
        if (dir->current_is_dir()) {
            files.append_array(_get_file_path_list(file_path, localized_file_path));
        }
        else {
            files.append(localized_file_path);
        }
    }

    return files;
}

CodeEdit *MultiGodot::_get_code_editor() {
    ScriptEditorBase* script_editor_container = ScriptEditor::get_singleton()->_get_current_editor();
    if (script_editor_container == nullptr) {
        return nullptr;
    }
    return script_editor_container->get_code_editor()->get_text_editor();
}

void MultiGodot::_create_lobby() {
    if (lobby_id == 0) {
        is_lobby_owner = true;
        steam->createLobby(LOBBY_TYPE_PUBLIC, MAX_MEMBERS);
    }
}

void MultiGodot::_join_lobby(uint64_t this_lobby_id) {
    if (VERBOSE_DEBUG) {
        print_line("Attempting to join lobby " + steam->getLobbyData(this_lobby_id, "name"));
    }

    lobby_members.clear();

    steam->joinLobby(this_lobby_id);
}

void MultiGodot::_get_lobby_members() {
    lobby_members.clear();
    steam_ids.clear();

    int num_members = steam->getNumLobbyMembers(lobby_id);

    for (int this_member = 0; this_member < num_members; this_member++) {
        uint64_t member_steam_id = steam->getLobbyMemberByIndex(lobby_id, this_member);
        String member_steam_name = steam->getFriendPersonaName(member_steam_id);

        auto data_hashmap = HashMap<String, Variant>(2);
        data_hashmap["steam_id"] = member_steam_id;
        data_hashmap["steam_name"] = member_steam_name;

        lobby_members.append(data_hashmap);
        steam_ids.append(member_steam_id);
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

void MultiGodot::_send_p2p_packet(uint64_t this_target, Dictionary packet_data, P2PSend custom_send_type, int custom_channel) {
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

void MultiGodot::_send_p2p_packet(uint64_t this_target, Dictionary packet_data) {
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

        uint64_t packet_sender = this_packet["remote_steam_id"];
        PackedByteArray packet_code = this_packet["data"];
        Dictionary readable_data = VariantUtilityFunctions::bytes_to_var(packet_code);

        if (!handshake_completed_with.has(packet_sender)) {
            handshake_completed_with.append(packet_sender);
            user_data.insert(packet_sender, HashMap<String, Variant>());
        }

        // ALWAYS have message value. 
        String message = readable_data["message"];
        if (message == "handshake") {
            if (VERBOSE_DEBUG) {
                print_line("Handshake completed with:");
                print_line(packet_sender);
            }
            if (!steam_ids.has(packet_sender)) {
                _get_lobby_members();
                _make_p2p_handshake();
            }
            if (is_lobby_owner) {
                _sync_filesystem();
            }
            _call_func(this, "_sync_user_data", {steam_id, _hashmap_to_dictionary(user_data.get(steam_id))});
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

        if (message == "call_func") {
            NodePath path = readable_data["path"];
            String function_name = readable_data["function_name"];
            Array args = readable_data["args"];
            Node *node = editor_node_singleton->get_node_or_null(path);
            if (!node) {
                print_error("Remote function call on null node at path " + String(path));
            }
            node->callv(function_name, args);
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

void MultiGodot::_sync_var(Node *node, StringName property, uint64_t custom_target) {
    NodePath path = editor_node_singleton->get_path_to(node);
    Variant value = editor_node_singleton->get_node(path)->get(property);
    auto send_dictionary = Dictionary({
        KeyValue<Variant, Variant>("message", "sync_var"),
        KeyValue<Variant, Variant>("path", path),
        KeyValue<Variant, Variant>("property", property),
        KeyValue<Variant, Variant>("value", value),
    });
    _send_p2p_packet(custom_target, send_dictionary);
}

void MultiGodot::_call_func(Node *node, String function_name, Array args, uint64_t custom_target) {
    NodePath path = editor_node_singleton->get_path_to(node);
    auto send_dictionary = Dictionary({
        KeyValue<Variant, Variant>("message", "call_func"),
        KeyValue<Variant, Variant>("path", path),
        KeyValue<Variant, Variant>("function_name", function_name),
        KeyValue<Variant, Variant>("args", args),
    });
    _send_p2p_packet(custom_target, send_dictionary);
}

void MultiGodot::_sync_scripts() {
    Ref<Script> current_script = script_editor->_get_current_script();

    if (current_script == nullptr) {
        return;
    }

    String current_code = current_script->get_source_code();
    String path = current_script->get_path();

    if (last_code == current_code) {
        return;
    }
    last_code = current_code;

    HashMap<String, Variant> this_data = user_data.get(steam_id);
    if (!this_data.has("editor_tab_index") || (int)this_data.get("editor_tab_index") != SCRIPT_EDITOR) {
        return;
    } 

    print_line("Detected a change in a script ... syncing to clients.");

    for (int i = 0; i < handshake_completed_with.size(); i++) {
        uint64_t this_lobby_member = handshake_completed_with[i];
        if (this_lobby_member == steam_id) { 
            continue;
        }
        HashMap<String, Variant> member_info = user_data.get(this_lobby_member);
        if (!member_info.has("current_script_path") || !member_info.has("editor_tab_index")) {
            print_error("Trying to sync a script with a client but they are missing info: either current_script_path or editor_tab_index.");
        }
        else if ((int)member_info.get("editor_tab_index") != SCRIPT_EDITOR || member_info.get("current_script_path") != (Variant)path) {
            _call_func(this, "_update_script_different", {path, current_code}, this_lobby_member);
        }
    }
}

void MultiGodot::_sync_live_edits_skewed() {
    CodeEdit *editor = _get_code_editor();
    Ref<Script> current_script = script_editor->_get_current_script();
    if (editor == nullptr || current_script == nullptr) {
        return;
    }

    String path = current_script->get_path();
    String code = editor->get_text();
    if (code == live_last_code) {
        return;
    }

    HashMap<String, Variant> this_data = user_data[steam_id];
    if ((int)this_data["editor_tab_index"] != SCRIPT_EDITOR) {
        return;
    }
    if ((String)this_data["current_script_path"] == "") {
        editor->set_text(live_last_code);
        live_last_code = code;
        return;
    }
    live_last_code = code;

    for (int i = 0; i < handshake_completed_with.size(); i++) {
        uint64_t other_id = handshake_completed_with[i];
        HashMap<String, Variant> other_data = user_data[other_id];
        if (other_id == steam_id || (String)other_data["current_spectating_script"] != path) {
            continue;
        }

        if ((String)other_data["current_script_path"] == path) { // We probably joined in the same script and didn't realize ...
            _on_current_script_path_changed(path);
            return;
        }

        _call_func(this, "_update_script_skewed", {steam_id, code});
    }
}

void MultiGodot::_sync_live_edits() {
    // Unfinished method for two people to edit at once. I think for now I'm just going to do
    // only one person can edit and they "own" the file.
    CodeEdit *editor = _get_code_editor();
    Ref<Script> current_script = script_editor->_get_current_script();
    if (editor == nullptr || current_script == nullptr) {
        return;
    }

    int line = editor->get_caret_line();
    String line_text = editor->get_line(line);

    Vector<int> occupied_lines;
    for (int i = 0; i < handshake_completed_with.size(); i++) {
        HashMap<String, Variant> this_data = user_data[handshake_completed_with[i]];
        if (this_data.has("script_current_line")) {
            int occupied = this_data.get("script_current_line");
            editor->set_line_background_color(occupied, LINE_OCCUPIED_BG_COLOR); // Doesn't work
            occupied_lines.append(occupied);
        }
    }

    bool newline = false;
    if (Input::get_singleton()->is_key_pressed(Key::ENTER) && !editor->is_code_completion_enabled()) {
        if (!was_enter_pressed) {
            newline = true;
        }
        was_enter_pressed = true;
    }
    else {
        was_enter_pressed = false;
    }

    // Not the best way to check if a line was just deleted -- what about selections?
    bool deleted_line = false;
    if (Input::get_singleton()->is_key_pressed(Key::BACKSPACE) && script_editor_previous_line_text.is_empty() && editor->get_line_count() < script_editor_previous_length) {
        deleted_line = true;
    }

    script_editor_previous_length = editor->get_line_count(); // Might need to move this forward in the future...

    if (line != script_editor_previous_line) {
        if (occupied_lines.has(line) && !newline) {
            editor->set_caret_line(script_editor_previous_line);
            return;
        }
        script_editor_previous_line = line;
        script_editor_previous_line_text = line_text;
        _set_user_data_for_everyone("script_current_line", line);
        if (!newline && !deleted_line) {
            return; // A line change isn't a text change, and since the text will be the same, just return.
        }
    }

    if (line_text == script_editor_previous_line_text && !newline && !deleted_line) {
        return;
    }
    
    for (int i = 0; i < handshake_completed_with.size(); i++) {
        uint64_t this_lobby_member = handshake_completed_with[i];
        if (this_lobby_member == steam_id) {
            continue;
        }
        HashMap<String, Variant> member_info = user_data.get(this_lobby_member);
        if (!member_info.has("current_script_path") || !member_info.has("editor_tab_index")) {
            print_error("Trying to sync a script with a client but they are missing info: either current_script_path or editor_tab_index.");
            continue;
        }
        if ((int)member_info.get("editor_tab_index") != SCRIPT_EDITOR || member_info.get("current_script_path") != (Variant)current_script->get_path()) {
            continue;
        }
        _call_func(this, "_update_script_same", {line, line_text, newline, deleted_line, script_editor_previous_line, script_editor_previous_column});
    }

    script_editor_previous_line_text = line_text;
    script_editor_previous_column = editor->get_caret_column();
}

void MultiGodot::_sync_filesystem() {
    String project_path = ProjectSettings::get_singleton()->get_resource_path();
    Vector<String> path_list = _get_file_path_list(project_path, "res:/");
    _call_func(this, "_compare_filesystem", {path_list, steam_id});
}

void MultiGodot::_sync_created_deleted_files() {
    mutex.lock();
    Vector<String> created = new_files;
    Vector<String> deleted = deleted_files;
    new_files = {};
    deleted_files = {};
    mutex.unlock();

    // Having a - after the preiod likely means the filename is something like script.gd-dyqRKE
    if (created.size() > 0 && deleted.size() > 0 && created[0] != deleted[0] && !created[0].split(".")[-1].contains_char('-')) {
        if (VERBOSE_DEBUG) {
            print_line("File with path " + deleted[0] + " was renamed to file with path " + created[0]);
        }
        _call_func(this, "_rename_file", {deleted[0], created[0]});
    }

    for (int i = 0; i < created.size(); i++) {
        String path = created[i];
        if (deleted.has(path)) {
            continue;
        }
        if (VERBOSE_DEBUG) {
            print_line("New file created: " + path);
        }
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
        _call_func(this, "_receive_file_contents", {path, file->get_as_text()});
        file->close();
    }

    for (int i = 0; i < deleted.size(); i++) {
        String path = deleted[i];
        if (created.has(path)) {
            continue;
        }
        if (VERBOSE_DEBUG) {
            print_line("File deleted: " + path);
        }
        _call_func(this, "_delete_file", {path});
    }
}

void MultiGodot::_set_user_data_for_everyone(String item, Variant value) {
    _set_user_data(steam_id, item, value);
    _call_func(this, "_set_user_data", {steam_id, item, value});
}

// REMOTE CALLABLES

void MultiGodot::_set_mouse_position(uint64_t sender, Vector2 pos) { 
    mouse_positions.insert(sender, pos);
}

void MultiGodot::_set_user_data(uint64_t sender, String item, Variant value) {
    if (VERBOSE_DEBUG) {
        print_line("User data set: " + item + " set to " + (String)value);
    }
    if (unlikely(!user_data.has(sender))) {
        print_error("A sender somehow isn't in the user_data hashmap.");
        return;
    }
    user_data.get(sender).insert(item, value);
}

void MultiGodot::_update_script_different(String path, String remote_code) {
    if (VERBOSE_DEBUG) {
        print_line("A client requested to update script at path " + path);
    }

    Ref<Script> current_script = script_editor->_get_current_script();
    HashMap<String, Variant> this_data = user_data.get(steam_id);
    if (!this_data.has("editor_tab_index")) {
        print_error("Missing data required to know if _update_script_different call is valid: editor_tab_index");
    }
    else if (unlikely(current_script != nullptr && current_script->get_path() == path && (int)this_data.get("editor_tab_index") == SCRIPT_EDITOR)) {
        print_error("Tried to save a script that was supposedly different from the current one but actually isn't.");
    }
    else {
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
        file->store_string(remote_code);
        script_editor->reload_scripts();
    }
}

void MultiGodot::_update_script_same(int line, String line_text, bool newline, bool deleted_line, int action_line, int action_column) {
    CodeEdit *editor = ScriptEditor::get_singleton()->_get_current_editor()->get_code_editor()->get_text_editor();
    if (editor == nullptr) {
        return;
    }

    if (editor->get_caret_line() == line && !newline) {
        print_error("Another client requested to update a line on the opened script, but we are also on that line.");
        return;
    }

    if (newline || deleted_line) {
        int previous_caret_line = editor->get_caret_line();
        int previous_caret_column = editor->get_caret_column();
        editor->set_caret_line(action_line, false);
        editor->set_caret_column(action_column, false);

        if (newline) {
            editor->_new_line();
            if (previous_caret_line >= action_line) {
                previous_caret_line++;
            }
        }

        if (deleted_line) {
            editor->set_line(action_line, ""); // Technically shouldn't be neccessary but just in case.
            editor->backspace();
            if (previous_caret_line > action_line) {
                previous_caret_line--;
            }
        }

        editor->set_caret_line(previous_caret_line, false);
        editor->set_caret_column(previous_caret_column, false);
    }


    editor->set_line(line, line_text);
}

void MultiGodot::_update_script_same_skewed(uint64_t from, String contents) {
    String this_spectating = user_data[steam_id]["current_spectating_script"];
    String other_editing = user_data[from]["current_script_path"];
    if (this_spectating == "" || this_spectating != other_editing) {
        print_error("Requested to update script as a spectator, but we are spectating " + this_spectating + " and they are editing " + other_editing);
        return;
    }

    CodeEdit *editor = _get_code_editor();
    if (editor == nullptr) {
        print_error("Editor is null"); // This should never happen
        return;
    }
    editor->set_text(contents);
}

void MultiGodot::_compare_filesystem(Vector<String> other_path_list, uint64_t host_id) {
    print_line("The host requested to compare and syncronize filesystems.");

    String project_path = ProjectSettings::get_singleton()->get_resource_path();
    Vector<String> this_path_list = _get_file_path_list(project_path, "res:/");

    Vector<String> missing_files;
    Vector<String> excess_files;

    // Start by comparing theirs to ours, to see if we have any missing files.
    for (int i = 0; i < other_path_list.size(); i++) {
        String path = other_path_list[i];
        if (this_path_list.has(path)) {
            continue;
        }

        missing_files.append(path);
    }

    // Now compare ours to theirs, to see if we have any excess files.
    for (int i = 0; i < this_path_list.size(); i++) {
        String path = this_path_list[i];
        if (other_path_list.has(path)) {
            continue;
        }

        excess_files.append(path);
    }

    // Add in missing files, which are empty and will be filled later.
    for (int i = 0; i < missing_files.size(); i++) {
        String path = missing_files[i];
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
        file->close();
        this_path_list.append(path);
    }

    // Remove excess files.
    for (int i = 0; i < excess_files.size(); i++) {
        String path = excess_files[i];
        Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        
        if (dir->file_exists(path)) {
            Error error = dir->remove(path);
            if (error != OK) {
                print_error("Error while deleting file with path " + path + ". Code: " + error);
            }
        }
        else {
            print_error("While removing an excess file, somehow the file with path " + path + " no longer exists.");
        }

        this_path_list.remove_at(this_path_list.find(path));
    }

    print_line("Debug: Printing our file path list.");
    for (int i = 0; i < this_path_list.size(); i++) {
        print_line(this_path_list[i]);
    }
    print_line("Debug: Printing remote file path list.");
    for (int i = 0; i < other_path_list.size(); i++) {
        print_line(other_path_list[i]);
    }

    // Requiest individual file contents from the host.
    _call_func(this, "_request_file_contents", {steam_id}, host_id);
}

void MultiGodot::_sync_user_data(uint64_t user_id, Dictionary individual_data) {
    if (VERBOSE_DEBUG) {
        print_line("Request from a client to overwrite user data.");
        print_line(individual_data);
    }
    user_data.insert(user_id, _dictionary_to_hashmap(individual_data));
}

void MultiGodot::_request_file_contents(uint64_t client_id) {
    if (!is_lobby_owner) {
        print_error("Request to get file contents to a non-host.");
        return;
    }
    else {
        print_line("A client requested all file contents.");
    }

    String project_path = ProjectSettings::get_singleton()->get_resource_path();
    Vector<String> path_list = _get_file_path_list(project_path, "res:/");

    for (int i = 0; i < path_list.size(); i++) {
        String path = path_list[i];
        Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
        int64_t size = FileAccess::get_size(path);

        // File can be sent as an individual packet.
        if (size < PACKET_SIZE_LIMIT) {
            String file_contents = file->get_as_text();
            _call_func(this, "_receive_file_contents", {path, file_contents}, client_id);
        }
        else {
            print_error("File size is greater than packet limit for path " + path);
        }
    }
}

void MultiGodot::_receive_file_contents(String path, String contents) {
    print_line("Received updated file contents for path " + path);
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    file->store_string(contents);
    EditorInterface::get_singleton()->get_resource_file_system()->scan();

    if (script_editor != nullptr) {
        script_editor->reload_scripts();
    }
}

void MultiGodot::_delete_file(String path) {
    print_line("Remote request to delete file at path " + path);

    Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    
    if (dir->file_exists(path)) {
        Error error = dir->remove(path);
        if (error != OK) {
            print_error("Error while deleting file with path " + path + ". Code: " + error);
        }
        else {
            EditorInterface::get_singleton()->get_resource_file_system()->scan();
        }
    }
    else {
        print_error("While deleting a file as requested by the host, somehow the file with path " + path + " no longer exists.");
    }
}

void MultiGodot::_rename_file(String from, String to) {
    print_line("Remote request to rename file from " + from + " to " + to);
    Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    dir->rename(from, to);
    EditorInterface::get_singleton()->get_resource_file_system()->scan();
}

// SIGNALS

void MultiGodot::_on_lobby_created(int connect, uint64_t this_lobby_id) {
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

void MultiGodot::_on_lobby_joined(uint64_t this_lobby_id, int _permissions, bool _locked, int response) {
    print_line("This client joined a lobby.");

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

        print_error("Failed to join lobby associated with this project. Error: " + fail_reason);
    }
}

void MultiGodot::_on_lobby_chat_update(uint64_t this_lobby_id, uint64_t change_id, uint64_t making_change_id, int chat_state) {
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

void MultiGodot::_on_p2p_session_request(uint64_t remote_id) {
    String this_requester = steam->getFriendPersonaName(remote_id);
    if (VERBOSE_DEBUG) {
        print_line(this_requester + " is requesting a P2P session.");
    }

    steam->acceptP2PSessionWithUser(remote_id);

    _get_lobby_members();
    _make_p2p_handshake();
}
 
void MultiGodot::_on_p2p_session_connect_fail(uint64_t this_steam_id, int session_error) {
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

void MultiGodot::_on_editor_tab_changed(int index) {
    if (VERBOSE_DEBUG) {
        print_line("Editor tab changed. Index " + (String)(Variant)index);
    }
    int tab;
    switch (index) {
        case EditorMainScreen::EDITOR_2D: tab = VIEWPORT_2D; break;
        case EditorMainScreen::EDITOR_3D: tab = VIEWPORT_3D; break;
        case EditorMainScreen::EDITOR_SCRIPT: tab = SCRIPT_EDITOR; break;
    };
    _set_user_data_for_everyone("editor_tab_index", tab);
    if (tab == SCRIPT_EDITOR) {
        _set_user_data_for_everyone("current_script_path", "");
        _set_user_data_for_everyone("current_spectating_script", "");
    }
}

void MultiGodot::_on_current_script_path_changed(String path) {
    if (VERBOSE_DEBUG) {
        print_line("Current script path changed: " + path + ". Sending to clients.");
    }
    if (!ENABLE_SAME_FILE_EDITS) { // Set script path to empty if someone else is already on it.
        for (int i = 0; i < steam_ids.size(); i++) {
            uint64_t this_steam_id = steam_ids[i];
            if (this_steam_id == steam_id) {
                continue;
            }
            HashMap<String, Variant> this_user_data = user_data[this_steam_id];
            if ((String)this_user_data["current_script_path"] == path) { // Possibly Variant->String is an unsafe cast?
                _set_user_data_for_everyone("current_script_path", ""); // Because we are spectating the script it isn't really "our" script.
                _set_user_data_for_everyone("current_spectating_script", path);
                return;
            }
        }
    }
    _set_user_data_for_everyone("current_script_path", path);
    _set_user_data_for_everyone("current_spectating_script", "");
}

// PLUGIN

MultiGodotPlugin::MultiGodotPlugin() {
    multi_godot = memnew(MultiGodot);
    multi_godot->set_name("MultiGodot");
    EditorNode *node = EditorNode::get_singleton();
    node->add_child(multi_godot, true);
}