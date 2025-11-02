#include <Client/Ui/InGame/Loadout.hh>

#include <Client/Ui/Container.hh>
#include <Client/Ui/StaticText.hh>

#include <Client/StaticData.hh>

#include <Client/Game.hh>

#include <format>

using namespace Ui;

static constexpr float TITLE_FONT = 25.0f;
static constexpr float BODY_FONT = 14.0f;

Element *Ui::UiLoadout::petal_tooltips[PetalID::kNumPetals] = {nullptr};

static float get_reload_factor() {
    if (!Game::alive()) return 1;
    float factor = 1;
    Entity &player = Game::simulation.get_ent(Game::player_id);
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        auto const &attrs = PETAL_DATA[player.get_loadout_ids(i)].attributes;
        factor *= (1.0f - attrs.reload_reduction);
    }
    return factor;
}

static float get_damage_factor() {
    if (!Game::alive()) return 1;
    float factor = 1;
    Entity &player = Game::simulation.get_ent(Game::player_id);
    for (uint32_t i = 0; i < player.get_loadout_count(); ++i) {
        factor *= PETAL_DATA[player.get_loadout_ids(i)].attributes.extra_damage_factor;
    }
    return factor;
}

static Ui::Element *make_petal_stat_container(PetalID::T id) {
    std::vector<Ui::Element *> stats = {};
    struct PetalData const &petal_data = PETAL_DATA[id];
    struct PetalAttributes const &attrs = petal_data.attributes;
    if (petal_data.damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Damage:", { .fill = 0xffec6869 }),
            new Ui::DynamicText(BODY_FONT, [&](){
                return format_number(petal_data.damage * get_damage_factor());
            })
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (petal_data.health > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Health:", { .fill = 0xff7eef6d }),
            new Ui::StaticText(BODY_FONT, format_number(petal_data.health))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.armor > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Armor:", { .fill = 0xff777777 }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.armor))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.damage_reflection > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Damage Reflection:", { .fill = 0xff777777 }),
            new Ui::StaticText(BODY_FONT, format_pct(attrs.damage_reflection * 100))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.constant_heal > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Heal:", { .fill = 0xffff94c9 }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.constant_heal) + "/s")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.burst_heal > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Heal:", { .fill = 0xffff94c9 }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.burst_heal))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.poison_damage.damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Poison:", { .fill = 0xffce76db }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.poison_damage.time * attrs.poison_damage.damage) + " (" + format_number(attrs.poison_damage.damage) + "/s)")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_health > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Flower Health:", { .fill = 0xff7eef6d }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.extra_health))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_body_damage > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Body Damage:", { .fill = 0xffec6869 }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.extra_body_damage))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.poison_armor > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Poison Armor:", { .fill = 0xffce76db }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.poison_armor) + "/s")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_rotation_speed > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Rotation Speed:", { .fill = 0xffcae934 }),
            new Ui::StaticText(BODY_FONT, "+" + format_number(attrs.extra_rotation_speed) + " rad/s")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.spawns != MobID::kNumMobs && attrs.spawn_count == 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Contents:", { .fill = 0xffd2eb34 }),
            new Ui::StaticText(BODY_FONT, (petal_data.count > 1 ? format_number(petal_data.count) + "x " : "") + (MOB_DATA[attrs.spawns].name))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.spawns != MobID::kNumMobs && attrs.spawn_count > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Spawns:", { .fill = 0xffd2eb34 }),
            new Ui::StaticText(BODY_FONT, format_number(attrs.spawn_count) + "x " + (MOB_DATA[attrs.spawns].name))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.vision_factor < 1) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Extra Vision:", { .fill = 0xffcae934 }),
            new Ui::StaticText(BODY_FONT, format_pct(100 / attrs.vision_factor))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_range > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Attack Range:", { .fill = 0xffcae934 }),
            new Ui::StaticText(BODY_FONT, "+" + format_number(attrs.extra_range))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (attrs.extra_damage_factor > 1) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Extra Damage:", { .fill = 0xffec6869 }),
            new Ui::StaticText(BODY_FONT, "+"+format_pct(100 * (attrs.extra_damage_factor - 1)))
        }, 0, 5, { .h_justify = Style::Left }));
    }
        if (attrs.reload_reduction > 0) {
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Reload:", { .fill = 0xffcae934 }),
            new Ui::StaticText(BODY_FONT, format_pct(attrs.reload_reduction * 100))
        }, 0, 5, { .h_justify = Style::Left }));
    }
    if (id == PetalID::kPowder) {
        // Non-stacking movement speed buff
        stats.push_back(new Ui::HContainer({
            new Ui::StaticText(BODY_FONT, "Movement Speed:", { .fill = 0xffcae934 }),
            new Ui::StaticText(BODY_FONT, "+10%")
        }, 0, 5, { .h_justify = Style::Left }));
    }
    return new Ui::VContainer(stats, 0, 3, { .h_justify = Style::Left, .should_render = [](){ return Game::show_tooltip_stats; } });
}

static void make_petal_tooltip(PetalID::T id) {
    Element *tooltip = new Ui::VContainer({
        #ifdef DEBUG
        new Ui::HFlexContainer(
            new Ui::StaticText(TITLE_FONT, PETAL_DATA[id].name, { .fill = 0xffffffff, .h_justify = Style::Left }),
            new Ui::DynamicText(BODY_FONT, [=](){
                float reload = PETAL_DATA[id].reload * get_reload_factor();
                std::string rld_str = reload == 0 ? "" :
                    PETAL_DATA[id].attributes.secondary_reload == 0 ? std::format("{:.1f}s ⟳", reload) : 
                    std::format("{:.1f} + {:.1f}s ⟳", reload, PETAL_DATA[id].attributes.secondary_reload);
                return rld_str;
            }, { .fill = 0xffffffff, .v_justify = Style::Top }),
            5, 10, {}
        ),
        #else
        new Ui::StaticText(TITLE_FONT, PETAL_DATA[id].name, { .fill = 0xffffffff, .h_justify = Style::Left }),
        #endif
        new Ui::StaticText(BODY_FONT, RARITY_NAMES[PETAL_DATA[id].rarity], { .fill = RARITY_COLORS[PETAL_DATA[id].rarity], .h_justify = Style::Left }),
        new Ui::Element(0,10),
        new Ui::StaticText(BODY_FONT, PETAL_DATA[id].description, { .fill = 0xffffffff, .h_justify = Style::Left }),
        new Ui::Element(0,8, { .should_render = [](){ return Game::show_tooltip_stats; } }),
        make_petal_stat_container(id)
    }, 5, 2);
    tooltip->style.fill = 0x80000000;
    tooltip->style.round_radius = 6;
    tooltip->refactor();
    Ui::UiLoadout::petal_tooltips[id] = tooltip;
}

void Ui::make_petal_tooltips() {
        for (PetalID::T i = 0; i < PetalID::kNumPetals; ++i)
        make_petal_tooltip(i);
}
