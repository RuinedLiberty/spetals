#include <Client/Ui/TitleScreen/TitleScreen.hh>

#include <Client/Ui/InGame/GameInfo.hh>
#include <Client/Ui/ScrollContainer.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/Game.hh>
#include <Client/StaticData.hh>
#include <Client/Assets/Assets.hh>

#include <algorithm>
#include <cstring>
#include <array>

using namespace Ui;


static void compute_spawnables(std::array<uint8_t, MobID::kNumMobs> &mob_ok, std::array<uint8_t, PetalID::kNumPetals> &petal_ok) {
    mob_ok.fill(0);
    petal_ok.fill(0);
    // Baseline petals obtainable regardless of mob spawns
    petal_ok[PetalID::kBasic] = 1;
    petal_ok[PetalID::kUniqueBasic] = 1;
    petal_ok[PetalID::kPollen] = 1; // players can drop pollen on death

    // Direct zone spawns
    for (auto const &zone : MAP_DATA) {
        for (auto const &sc : zone.spawns) {
            if (sc.chance > 0) {
                if (sc.id < MobID::kNumMobs)
                    mob_ok[sc.id] = 1;
            }
        }
    }

    // Closure: propagate via AntHole waves, petal spawns, and mob drops
    bool changed;
    do {
        changed = false;
        // If AntHole spawns, include all wave mobs (e.g., Queen Ant)
        if (mob_ok[MobID::kAntHole]) {
            for (auto const &wave : ANTHOLE_SPAWNS) {
                for (MobID::T m : wave) {
                    if (m < MobID::kNumMobs && !mob_ok[m]) { mob_ok[m] = 1; changed = true; }
                }
            }
        }
        // Petals that spawn mobs make those mobs obtainable
        for (PetalID::T p = 0; p < PetalID::kNumPetals; ++p) {
            if (!petal_ok[p]) continue;
            auto const &attrs = PETAL_DATA[p].attributes;
            if (attrs.spawns != MobID::kNumMobs) {
                if (!mob_ok[attrs.spawns]) { mob_ok[attrs.spawns] = 1; changed = true; }
            }
        }
        // Mobs that are obtainable drop petals that become obtainable
        for (MobID::T m = 0; m < MobID::kNumMobs; ++m) {
            if (!mob_ok[m]) continue;
            auto const &md = MOB_DATA[m];
            for (auto const &pid : md.drops) {
                if (pid < PetalID::kNumPetals && !petal_ok[pid]) { petal_ok[pid] = 1; changed = true; }
            }
        }
    } while (changed);
}


GalleryPetal::GalleryPetal(PetalID::T id, float w) : 
    Element(w,w,{ .fill = 0x40000000, .round_radius = w/20 , .h_justify = Style::Left }), id(id) {}

void GalleryPetal::on_render(Renderer &ctx) {
    if (!Game::seen_petals[id]) {
        Element::on_render(ctx);
        ctx.draw_text("?", { .fill = 0xffeeeeee, .size = width / 2, .stroke_scale = 0});
    } else {
        ctx.scale(width / 60);
        draw_loadout_background(ctx, id);
    }
}

void GalleryPetal::on_event(uint8_t event) {
    if (event != kFocusLost && id != PetalID::kNone && Game::seen_petals[id]) {
        rendering_tooltip = 1;
        tooltip = Ui::UiLoadout::petal_tooltips[id];
    } else
        rendering_tooltip = 0;
}

PetalsCollectedIndicator::PetalsCollectedIndicator(float w) : Element(w,w,{}) {}

void PetalsCollectedIndicator::on_render(Renderer &ctx) {
    uint32_t colct = 0;
    uint32_t totct = 0;
    for (PetalID::T i = PetalID::kBasic; i < PetalID::kNumPetals; ++i) {
        colct += Game::seen_petals[i] != 0;
        ++totct;
    }
    ctx.set_fill(0x80000000);
    ctx.begin_path();
    ctx.arc(0,0,width/2);
    ctx.fill();
    ctx.set_fill(0xc0eeeeee);
    ctx.begin_path();
    ctx.move_to(0,0);
    ctx.partial_arc(0,0,width/2*0.8,-M_PI/2,-M_PI/2+2*M_PI*colct/totct,0);
    ctx.close_path();
    ctx.fill();
}

static Element *make_scroll() {
    Element *elt = new Ui::VContainer({}, 10, 10, {});

    std::array<uint8_t, MobID::kNumMobs> mob_ok;
    std::array<uint8_t, PetalID::kNumPetals> petal_ok;
    compute_spawnables(mob_ok, petal_ok);

    std::vector<PetalID::T> ids;
    for (PetalID::T i = PetalID::kBasic; i < PetalID::kNumPetals; ++i) {
        if (petal_ok[i]) ids.push_back(i);
    }

    std::sort(ids.begin(), ids.end(), [](PetalID::T a, PetalID::T b){
        if (PETAL_DATA[a].rarity < PETAL_DATA[b].rarity) return true;
        if (PETAL_DATA[a].rarity > PETAL_DATA[b].rarity) return false;
        return strcmp(PETAL_DATA[a].name, PETAL_DATA[b].name) <= 0;
    });

    for (size_t i = 0; i < ids.size();) {
        Element *row = new Ui::HContainer({}, 0, 10, { .v_justify = Style::Top });
        for (uint8_t j = 0; j < 4 && i < ids.size(); ++j, ++i) {
            row->add_child(new GalleryPetal(ids[i], 60));
        }
        row->refactor();
        elt->add_child(row);
    }
    return new Ui::ScrollContainer(elt, 300);
}


Element *Ui::make_petal_gallery() {
    Element *elt = new Ui::VContainer({
        new Ui::StaticText(25, "Petal Gallery"),
        make_scroll()
    }, 15, 10, { 
        .fill = 0xff5a9fdb,
        .line_width = 7,
        .round_radius = 3,
        .animate = [](Element *elt, Renderer &ctx){
            ctx.translate(0, (1 - elt->animation) * 2 * elt->height);
        },
        .should_render = [](){
            return Ui::panel_open == Panel::kPetals && Game::should_render_title_ui();
        },
        .h_justify = Style::Left,
        .v_justify = Style::Bottom
    });
    Ui::Panel::petal_gallery = elt;
    return elt;
}