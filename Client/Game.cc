#include <Client/Game.hh>

#include <Client/Debug.hh>
#include <Client/Input.hh>
#include <Client/Particle.hh>
#include <Client/Setup.hh>
#include <Client/Storage.hh>

#include <Shared/Config.hh>

#include <cmath>
#include <emscripten.h>

extern "C" {
EM_JS(void, update_logged_in_as, (), {
  if (typeof fetch !== 'function') { Module.logged_in_as = ""; Module.isLoggedIn = false; return; }
  fetch('/api/me', { credentials: 'include' })
    .then(function(r){
      if (!r) return { ok:false };
      var ok = !!r.ok;
      return r.json().then(function(j){ return { ok: ok, j: j }; }).catch(function(){ return { ok: ok, j: null }; });
    })
        .then(function(res){
      var ok = res && res.ok;
      var j = res && res.j || {};
      if (ok) {
        var name = (j && (j.username || (j.user && j.user.username) || j.name || j.global_name)) || '';
        var id = (j && (j.discord_id || j.id || (j.user && j.user.id))) || '';
        if (name || id) {
          Module.logged_in_as = 'Logged in as ' + (name || 'User') + (id ? ' ('+id+')' : '');
        } else {
          Module.logged_in_as = '';
        }
        // Treat any ok response as logged in, even if label is empty
        Module.isLoggedIn = true;
      } else {
        Module.logged_in_as = "";
        Module.isLoggedIn = false;
      }
    })
    .catch(function(){ Module.logged_in_as = ""; Module.isLoggedIn = false; });
});

EM_JS(char*, get_logged_in_as, (), {
  var s = Module.logged_in_as || "";
  var len = lengthBytesUTF8(s) + 1;
  var p = _malloc(len);
  stringToUTF8(s, p, len);
  return p;
});

EM_JS(int, get_is_logged_in, (), {
  return Module.isLoggedIn ? 1 : 0;
});
}



static double g_last_time = 0;
float const MAX_TRANSITION_CIRCLE = 2500;

static int _c = setup_canvas();
static int _i = setup_inputs();

namespace Game {
    Simulation simulation;
    Renderer renderer;
    Renderer game_ui_renderer;
    Socket socket;
    Ui::Window title_ui_window;
    Ui::Window game_ui_window;
    Ui::Window other_ui_window;
    EntityID camera_id;
    EntityID player_id;
    std::string nickname;
    std::string disconnect_message;
    std::array<uint8_t, PetalID::kNumPetals> seen_petals;
    std::array<uint8_t, MobID::kNumMobs> seen_mobs;
    std::array<PetalID::T, 2 * MAX_SLOT_COUNT> cached_loadout = {PetalID::kNone};

    double timestamp = 0;

    double score = 0;
    float overlevel_timer = 0;
    float slot_indicator_opacity = 0;
    float transition_circle = 0;

    uint32_t respawn_level = 1;
    uint32_t account_level = 1;
    uint32_t account_xp = 0;
    uint32_t account_xp_needed = 1;




    uint8_t loadout_count = 5;
    uint8_t simulation_ready = 0;
    uint8_t on_game_screen = 0;
    uint8_t show_debug = 0;
    uint8_t show_tooltip_stats = 0;
}

using namespace Game;

void Game::init() {
    Input::is_mobile = check_mobile();
            Storage::retrieve();
    reset();


        // Prime login state immediately
    update_logged_in_as();
    // Clear galleries until server seeds them (avoid stale local data when not logged in)
    for (PetalID::T i = 0; i < PetalID::kNumPetals; ++i) Game::seen_petals[i] = 0;
    for (MobID::T i = 0; i < MobID::kNumMobs; ++i) Game::seen_mobs[i] = 0;
    Game::seen_petals[PetalID::kBasic] = 1;
    title_ui_window.add_child(


        [](){ 
            Ui::Element *elt = new Ui::StaticText(60, "spetals.io");
            elt->x = 0;
            elt->y = -270;
            return elt;
        }()
    );
    title_ui_window.add_child(
        Ui::make_title_input_box()
    );
    title_ui_window.add_child(
        Ui::make_title_info_box()
    );
    // Logged-in-as (top-left)
    title_ui_window.add_child([](){
        return (Ui::Element*) new Ui::HContainer({
            new Ui::DynamicText(14, [](){
                static double last = -1e9;
                if (Game::timestamp - last > 2000) { last = Game::timestamp; update_logged_in_as(); }
                char *ptr = get_logged_in_as();
                std::string out;
                if (ptr && *ptr) { out = std::string(ptr); free(ptr); }
                return out;
            }, { .fill = 0xffffffff })
        }, 10, 10, { .h_justify = Ui::Style::Left, .v_justify = Ui::Style::Top });
    }());

    title_ui_window.add_child(
        Ui::make_panel_buttons()
    );
    title_ui_window.add_child(
        Ui::make_settings_panel()
    );
    title_ui_window.add_child(
        Ui::make_petal_gallery()
    );
    title_ui_window.add_child(
        Ui::make_mob_gallery()
    );
    title_ui_window.add_child(
        Ui::make_changelog()
    );
    title_ui_window.add_child(
        Ui::make_github_link_button()
    );
    game_ui_window.add_child(
        Ui::make_death_main_screen()
    );
    game_ui_window.add_child(
        Ui::make_level_bar()
    );
    game_ui_window.add_child(
        Ui::make_minimap()
    );
    game_ui_window.add_child(
        Ui::make_loadout_backgrounds()
    );
    game_ui_window.add_child(
        Ui::make_mobile_joystick()
    );
    game_ui_window.add_child(
        Ui::make_mobile_attack_button()
    );
    game_ui_window.add_child(
        Ui::make_mobile_defend_button()
    );
    for (uint8_t i = 0; i < MAX_SLOT_COUNT * 2; ++i) game_ui_window.add_child(new Ui::UiLoadoutPetal(i));
    game_ui_window.add_child(
        Ui::make_leaderboard()
    );
    game_ui_window.add_child(
        Ui::make_overlevel_indicator()
    );
    game_ui_window.add_child(
        Ui::make_stat_screen()
    );
    game_ui_window.add_child(
        new Ui::HContainer({
            new Ui::StaticText(20, "spetals.io")
        }, 20, 0, { .h_justify = Ui::Style::Left, .v_justify = Ui::Style::Top })
    );
    Ui::make_petal_tooltips();
    other_ui_window.add_child(
        Ui::make_debug_stats()
    );
    other_ui_window.add_child(
        [](){ 
            Ui::Element *elt = new Ui::HContainer({
                new Ui::DynamicText(16, [](){ return Game::disconnect_message; })
            }, 5, 5, { 
                .fill = 0x40000000, 
                .should_render = [](){
                    return !Game::socket.ready && Game::disconnect_message != "";
                },
                .v_justify = Ui::Style::Top
            });
            elt->y = 50;
            return elt;
        }()
    );
    other_ui_window.style.no_polling = 1;
    socket.connect(WS_URL);
}

void Game::reset() {
    simulation_ready = 0;
    on_game_screen = 0;
    score = 0;
    overlevel_timer = 0;
    slot_indicator_opacity = 0;
    transition_circle = 0;
    respawn_level = 1;
    loadout_count = 5;
    camera_id = player_id = NULL_ENTITY;
    for (uint32_t i = 0; i < 2 * MAX_SLOT_COUNT; ++i)
        cached_loadout[i] = PetalID::kNone;
    simulation.reset();
}

uint8_t Game::alive() {
    return socket.ready && simulation_ready
    && simulation.ent_exists(camera_id)
    && simulation.ent_alive(simulation.get_ent(camera_id).get_player());
}

uint8_t Game::in_game() {
    return simulation_ready && on_game_screen
    && simulation.ent_exists(camera_id);
}

uint8_t Game::should_render_title_ui() {
    return transition_circle < MAX_TRANSITION_CIRCLE;
}

uint8_t Game::should_render_game_ui() {
    return transition_circle > 0 && simulation_ready && simulation.ent_exists(camera_id);
}

void Game::poll_ui_event(Ui::ScreenEvent const &event) {
    Ui::focused = nullptr;
    if (Game::should_render_title_ui())
        title_ui_window.poll_events(event);
    if (Game::should_render_game_ui())
        game_ui_window.poll_events(event);
    other_ui_window.poll_events(event);
    if (Ui::focused != nullptr)
        Ui::focused->focused = 1;
}

void Game::tick(double time) {
    double tick_start = Debug::get_timestamp();
    Game::timestamp = time;
    Ui::dt = time - g_last_time;
    Ui::lerp_amount = 1 - pow(1 - 0.2, Ui::dt * 60 / 1000);
    g_last_time = time;
    simulation.tick();
    
    renderer.reset();
    game_ui_renderer.set_dimensions(renderer.width, renderer.height);
    game_ui_renderer.reset();

    Ui::window_width = renderer.width;
    Ui::window_height = renderer.height;
    Ui::focused = nullptr;
    double a = Ui::window_width / 1920;
    double b = Ui::window_height / 1080;
    Ui::scale = std::max(a, b);
        // Update beforeunload guard based on in-game state
    EM_ASM({
        try {
            if ($0) {
                window.onbeforeunload = function(e){ return "Are you sure?"; };
            } else {
                window.onbeforeunload = null;
            }
        } catch(e) {}
    }, Game::in_game());

    if (alive()) {

        on_game_screen = 1;
        player_id = simulation.get_ent(camera_id).get_player();
        Entity const &player = simulation.get_ent(player_id);
        Game::loadout_count = player.get_loadout_count();
        for (uint32_t i = 0; i < MAX_SLOT_COUNT + Game::loadout_count; ++i) {
            cached_loadout[i] = player.get_loadout_ids(i);
            Game::seen_petals[cached_loadout[i]] = 1;
        }
        score = player.get_score();
        overlevel_timer = player.get_overlevel_timer();
    } else {
        player_id = NULL_ENTITY;
        overlevel_timer = 0;
    }

    //event poll
    if (Input::is_mobile) {
        for (auto &x : Input::touches) {
            Input::Touch const &touch = x.second;
            if (touch.saturated) continue;
            Game::poll_ui_event({ .id = touch.id, .x = touch.x, .y = touch.y, .press = 1 });
        }
    }
    else {
        Game::poll_ui_event({ .id = 0, .x = Input::mouse_x, .y = Input::mouse_y, .press = 0 });
    }

    if (in_game())
        transition_circle = fclamp(transition_circle * powf(1.05, Ui::dt * 60 / 1000) + Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);
    else 
        transition_circle = fclamp(transition_circle / powf(1.05, Ui::dt * 60 / 1000) - Ui::dt / 5, 0, MAX_TRANSITION_CIRCLE);

    if (should_render_title_ui()) {
        render_title_screen();
        Particle::tick_title(renderer, Ui::dt);
        title_ui_window.render(renderer);
    } else
        title_ui_window.on_render_skip(renderer);

    if (should_render_game_ui()) {
        RenderContext c(&renderer);
        if (should_render_title_ui()) {
            renderer.set_stroke(0xff222222);
            renderer.set_line_width(Ui::scale * 10);
            renderer.begin_path();
            renderer.arc(renderer.width / 2, renderer.height / 2, transition_circle);
            renderer.stroke();
            renderer.clip();
        }
        render_game();
        if (!Game::alive()) {
            RenderContext c(&renderer);
            renderer.reset_transform();
            renderer.set_fill(0x20000000);
            renderer.fill_rect(0,0,renderer.width,renderer.height);
        }
        game_ui_window.render(game_ui_renderer);
        renderer.set_global_alpha(0.85);
        renderer.translate(renderer.width/2,renderer.height/2);
        renderer.draw_image(game_ui_renderer);
        //process keybind petal switches
        if (Input::keys_held_this_tick.contains('X'))
            Game::swap_all_petals();
        else if (Input::keys_held_this_tick.contains('E')) 
            Ui::forward_secondary_select();
        else if (Input::keys_held_this_tick.contains('Q')) 
            Ui::backward_secondary_select();
        else if (Ui::UiLoadout::selected_with_keys == MAX_SLOT_COUNT) {
            for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                if (Input::keys_held_this_tick.contains(SLOT_KEYBINDS[i])) {
                    Ui::forward_secondary_select();
                    break;
                }
            }
        } else if (Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] == PetalID::kNone)
            Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        if (Ui::UiLoadout::selected_with_keys < MAX_SLOT_COUNT 
            && Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] != PetalID::kNone) {
            if (Input::keys_held_this_tick.contains('T')) {
                Ui::ui_delete_petal(Ui::UiLoadout::selected_with_keys + Game::loadout_count);
                Ui::forward_secondary_select();
            } else {
                for (uint8_t i = 0; i < Game::loadout_count; ++i) {
                    if (Input::keys_held_this_tick.contains(SLOT_KEYBINDS[i])) {
                        Ui::ui_swap_petals(i, Ui::UiLoadout::selected_with_keys + Game::loadout_count);
                        if (Game::cached_loadout[Game::loadout_count + Ui::UiLoadout::selected_with_keys] == PetalID::kNone)
                            Ui::forward_secondary_select();
                        break;
                    }
                }
            }
        }
    } else {
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
        game_ui_window.on_render_skip(game_ui_renderer);
    }
        
    if (Game::timestamp - Ui::UiLoadout::last_key_select > 5000)
        Ui::UiLoadout::selected_with_keys = MAX_SLOT_COUNT;
    slot_indicator_opacity = lerp(slot_indicator_opacity, Ui::UiLoadout::selected_with_keys != MAX_SLOT_COUNT, Ui::lerp_amount);

    other_ui_window.render(renderer);

    //no rendering past this point
    if (!Input::is_mobile) {
        if (Input::keyboard_movement) {
            Input::game_inputs.x = 300 * (Input::keys_held.contains('D') - Input::keys_held.contains('A') + Input::keys_held.contains(39) - Input::keys_held.contains(37));
            Input::game_inputs.y = 300 * (Input::keys_held.contains('S') - Input::keys_held.contains('W') + Input::keys_held.contains(40) - Input::keys_held.contains(38));
        } else {
           Input::game_inputs.x = (Input::mouse_x - renderer.width / 2) / Ui::scale;
           Input::game_inputs.y = (Input::mouse_y - renderer.height / 2) / Ui::scale;
        }
        uint8_t attack = Input::keys_held.contains(' ') || BitMath::at(Input::mouse_buttons_state, Input::LeftMouse);
        uint8_t defend = Input::keys_held.contains('\x10') || BitMath::at(Input::mouse_buttons_state, Input::RightMouse);
        Input::game_inputs.flags = (attack << InputFlags::kAttacking) | (defend << InputFlags::kDefending);
    }

    if (socket.ready && alive()) send_inputs();

    if (Input::keys_held_this_tick.contains(';'))
        show_debug = !show_debug;
    if (Input::keys_held_this_tick.contains('\r') && !Game::alive())
        Game::spawn_in();

    //clearing operations
    simulation.post_tick();
    Storage::set();
    Input::reset();
    Debug::frame_times.push_back(Ui::dt);
    Debug::tick_times.push_back(Debug::get_timestamp() - tick_start);
}