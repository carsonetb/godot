#ifndef MULTI_GODOT_H
#define MULTI_GODOT_H

#include "button_notifier.h"
#include "core/math/color.h"
#include "core/object/ref_counted.h"
#include "editor/editor_node.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "modules/godotsteam/godotsteam.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/code_edit.h"

class MultiGodot : public Node2D {
    GDCLASS(MultiGodot, Node2D);

    protected:
        enum RemoteMainScreenStatus {
            VIEWPORT_2D,
            VIEWPORT_3D,
            SCRIPT_EDITOR,
            MS_OTHER,
        };

        enum RemoteSidePanelStatus {
            INSPECTOR,
            NODE,
            HISTORY,
        };

        enum RemoteProjectSettingsStatus {
            PS_GENERAL,
            INPUT_MAP,
            LOCALIZATION,
            GLOBALS,
            PLUGINS,
            IMPORT_DEFAULTS,
        };

        enum RemoteProjectSettingsGeneralStatus {
            CONFIG,
            RUN,
            BOOT_SPLASH,
            GENERAL,
            WINDOW,
            MOUSE_CURSOR,
            BUSES,
            RENDERING,
            LOCALE,
            COMMON,
            FONTS,
            THEME,
            TEXTURES,
            RENDERER,
            VIEWPORT,
            ANTI_ALIASING,
            ENVIRONMENT,
            RENDERING_2D,
            POINTING,
            SENSORS,
            PHYSICS_COMMON,
            PHYSICS_2D,
            PHYSICS_3D,
            OPENXR,
            SHADERS,
            MOVIE_WRITER,
            INITIALIZATION,
            NAVIGATION_3D,
            NAVIGATION_2D,
            RENDER_2D,
            RENDER_3D,
            LAYER_NAMES_PHYSICS_2D,
            LAYER_NAMES_NAVIGATION_2D,
            LATER_NAMES_3D,
            IMPORT,
        };

        enum RemoteProjectSettingsLocalizationStatus {
            TRANSLATIONS,
            REMAPS,
            POT_GENERATION,
        };

        enum RemoteProjectSettingsGlobalsStatus {
            AUTOLOAD,
            SHADER_GLOBALS,
            GROUPS,
        };

        enum RemoteSceneViewStatus {
            SCENE,
            SV_IMPORT,
        };

        enum RemoteTabStatus {
            AUDIO,
            ANIMATION,
            SHADER_EDITOR,
            TS_OTHER,
        };

        static const int PACKET_READ_LIMIT = 32;
        static const int MAX_MEMBERS = 4; // Possibly increase this in the future if there is a need.
        static const int DEFAULT_CHANNEL = 0;
        static const int PACKET_SIZE_LIMIT = 1200; // bytes
        static const bool VERBOSE_DEBUG = true;
        static const P2PSend SEND_TYPE = P2P_SEND_UNRELIABLE;
        const Color LINE_OCCUPIED_BG_COLOR = Color::from_rgba8(36, 9, 14, 0.3);

        // NODES

        Steam *steam;
        ButtonNotifier *button_notifier;
        ScriptEditor *script_editor;
        EditorNode *editor_node_singleton;

        // PROPERTIES

        int script_editor_previous_line = 0;
        int script_editor_previous_column = 0;
        int script_editor_previous_length = 0;
        uint64_t lobby_id = 0;
        uint64_t steam_id = 0;
        bool is_lobby_owner = false;
        bool printed = false;
        bool stop_filesystem_scanner = false;
        bool was_enter_pressed = false;
        String this_project_name;
        String last_code;
        String script_editor_previous_line_text;
        Vector<HashMap<String, Variant>> lobby_members;
        Vector<String> new_files;
        Vector<String> deleted_files; // Both this and new_files will fill up if not cleaned up by the main thread.
        Vector<uint64_t> steam_ids;
        Thread filesystem_scanner;
        Mutex mutex;

        // REMOTE PROPERTIES

        HashMap<uint64_t, Vector2> mouse_positions;
        HashMap<uint64_t, HashMap<String, Variant>> user_data;
        Vector<uint64_t> handshake_completed_with;

        // BUILTINS

        static void _bind_methods();
        void _notification(int what);
        void _ready();
        void _process();
        void _exit_tree();
        void _draw();

        // METHODS

        static void _threaded_filesystem_scanner(void* p_userdata);
        static Dictionary _hashmap_to_dictionary(HashMap<String, Variant> map);
        static HashMap<String, Variant> _dictionary_to_hashmap(Dictionary dict);
        static Vector<String> _get_file_path_list(String path, String localized_path = "res:/");
        void _create_lobby();
        void _join_lobby(uint64_t this_lobby_id);
        void _get_lobby_members();
        void _make_p2p_handshake();
        void _send_p2p_packet(uint64_t this_target, Dictionary packet_data, P2PSend custom_send_type, int custom_channel);
        void _send_p2p_packet(uint64_t this_target, Dictionary packet_data);
        void _read_all_p2p_packets(int read_count);
        void _read_p2p_packet();
        void _leave_lobby();
        void _sync_var(Node *node, StringName property, uint64_t custom_target = 0);
        void _call_func(Node *node, String function_name, Array args, uint64_t custom_target = 0);
        void _sync_scripts();
        void _sync_live_edits();
        void _sync_filesystem();
        void _sync_created_deleted_files();

        // REMOTE CALLABLES

        void _set_mouse_position(uint64_t sender, Vector2 pos);
        void _set_user_data(uint64_t sender, String item, Variant value);
        void _update_script_different(String path, String code);
        void _update_script_same(int line, String line_text, bool newline, bool deleted_line, int indent_from_line, int indent_from_column);
        void _compare_filesystem(Vector<String> path_list, uint64_t host_id);
        void _request_file_contents(uint64_t client_id);
        void _receive_file_contents(String path, String contents);
        void _delete_file(String path);
        void _rename_file(String from, String to);
        void _sync_user_data(uint64_t user_id, Dictionary data);

        // SIGNALS

        void _on_lobby_created(int connect, uint64_t this_lobby_id);
        void _on_lobby_match_list(Array these_lobbies);
        void _on_lobby_joined(uint64_t this_lobby_id, int _permissions, bool _locked, int response);
        void _on_lobby_chat_update(uint64_t this_lobby_id, uint64_t change_id, uint64_t making_change_id, int chat_state);
        void _on_p2p_session_request(uint64_t remote_id);
        void _on_p2p_session_connect_fail(uint64_t this_steam_id, int session_error);
        void _on_editor_tab_changed(int index);
        void _on_current_script_path_changed(String path);
    
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