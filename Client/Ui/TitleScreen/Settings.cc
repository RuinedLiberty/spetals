#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/Button.hh>
#include <Client/Ui/Container.hh>
#include <Client/Ui/Extern.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/DOM.hh>
#include <Client/Game.hh>
#include <Client/Input.hh>
#include <cstdlib>


extern "C" {
    void update_logged_in_as();
    char* get_logged_in_as();
}

static bool is_logged_in_settings() {
    char *ptr = get_logged_in_as();
    bool ok = (ptr && *ptr);
    if (ptr) free(ptr);
    return ok;
}

using namespace Ui;

Element *Ui::make_settings_panel() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Settings"),
        new Ui::HContainer({
            new Ui::ToggleButton(30, &Input::keyboard_movement),
            new Ui::StaticText(16, "Keyboard movement")
        }, 0, 10, {.h_justify = Style::Left }),
        new Ui::HContainer({
            new Ui::ToggleButton(30, &Input::movement_helper),
            new Ui::StaticText(16, "Movement helper")
        }, 0, 10, {.h_justify = Style::Left }),
            new Ui::HContainer({
            new Ui::ToggleButton(30, &Game::show_debug),
            new Ui::StaticText(16, "Debug stats")
        }, 0, 10, {.h_justify = Style::Left }),
            new Ui::HContainer({
            new Ui::ToggleButton(30, &Game::show_tooltip_stats),
            new Ui::StaticText(16, "Tooltip stats")
        }, 0, 10, {.h_justify = Style::Left }),
        new Ui::Button(140, 40,
            new Ui::StaticText(16, "Logout"),
            [](Element *elt, uint8_t e){ if (e == Ui::kClick) DOM::open_page("/auth/logout"); },
            nullptr,
            { .fill = 0xffc23b22, .line_width = 5, .round_radius = 4, .should_render = [](){ return is_logged_in_settings(); } }
        ),
        new Ui::StaticText(12, "Made by bismuth (trigonal-bacon)"),
        new Ui::StaticText(12, "Asset credits: M28 and affiliates")
    }, 20, 10, { 
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kSettings && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::settings = elt;
    return elt;
}
