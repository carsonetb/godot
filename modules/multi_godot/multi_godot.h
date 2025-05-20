#ifndef MULTI_GODOT_H
#define MULTI_GODOT_H

#include "button_notifier.h"
#include "core/object/ref_counted.h"
#include "editor/editor_node.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "modules/godotsteam/godotsteam.h"
#include "scene/2d/node_2d.h"

class MultiGodot : public Node2D {
    GDCLASS(MultiGodot, Node2D);

    protected:
        #define RETURN_NULL(item) \
            if (item == nullptr) { return; }
        // RETURN_NULL

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
        static const bool VERBOSE_DEBUG = true;
        static const P2PSend SEND_TYPE = P2P_SEND_UNRELIABLE;
        static const int DEFAULT_CHANNEL = 0;

        // NODES

        Steam *steam;
        ButtonNotifier *button_notifier;
        ScriptEditor *script_editor;
        EditorNode *editor_node_singleton;

        uint64_t lobby_id = 0;
        uint64_t steam_id = 0;
        bool is_lobby_owner = false;
        String this_project_name;
        Vector<HashMap<String, Variant>> lobby_members;
        bool printed = false;
        String last_code;

        // Remote properties
        HashMap<uint64_t, Vector2> mouse_positions;
        HashMap<uint64_t, HashMap<String, Variant>> user_data;
        Vector<uint64_t> handshake_completed_with;

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
        void _sync_var(Node *node, StringName property, uint64_t custom_target = 0);
        void _call_func(Node *node, String function_name, Array args, uint64_t custom_target = 0);
        void _sync_scripts();

        // REMOTE CALLABLES

        void _set_mouse_position(uint64_t sender, Vector2 pos) {mouse_positions.insert(sender, pos);}
        void _set_user_data(uint64_t sender, String item, Variant value);
        void _update_script_different(String path, String code);

        // SIGNALS

        void _on_lobby_created(int connect, uint64_t this_lobby_id);
        void _on_lobby_match_list(Array these_lobbies);
        void _on_lobby_joined(uint64_t this_lobby_id, int _permissions, bool _locked, int response);
        void _on_lobby_chat_update(uint64_t this_lobby_id, uint64_t change_id, uint64_t making_change_id, int chat_state);
        void _on_p2p_session_request(uint64_t remote_id);
        void _on_p2p_session_connect_fail(uint64_t this_steam_id, int session_error);
        void _on_main_screen_selected(int index);
        void _on_script_tab_changed(String path);
    
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