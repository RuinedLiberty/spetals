#include <Client/Assets/Assets.hh>

#include <Client/StaticData.hh>
#include <Client/Assets/Petals/Petals.hh>

#include <cmath>

// NOTE: Routed to dedicated per-petal rendering functions only.
void draw_static_petal_single(PetalID::T id, Renderer &ctx) {
    float r = PETAL_DATA[id].radius;
    switch (id) {
        case PetalID::kNone: Petals::None(ctx, r); break;
        case PetalID::kBasic: Petals::Basic(ctx, r); break;
        case PetalID::kLight: Petals::Light(ctx, r); break;
        case PetalID::kHeavy: Petals::Heavy(ctx, r); break;
        case PetalID::kStinger: Petals::Stinger(ctx, r); break;
        case PetalID::kLeaf: Petals::Leaf(ctx, r); break;
        case PetalID::kTwin: Petals::Twin(ctx, r); break;
        case PetalID::kRose: Petals::Rose(ctx, r); break;
        case PetalID::kIris: Petals::Iris(ctx, r); break;
        case PetalID::kMissile: Petals::Missile(ctx, r); break;
        case PetalID::kDandelion: Petals::Dandelion(ctx, r); break;
        case PetalID::kBubble: Petals::Bubble(ctx, r); break;
        case PetalID::kFaster: Petals::Faster(ctx, r); break;
        case PetalID::kRock: Petals::Rock(ctx, r); break;
        case PetalID::kCactus: Petals::Cactus(ctx, r); break;
        case PetalID::kWeb: Petals::Web(ctx, r); break;
        case PetalID::kWing: Petals::Wing(ctx, r); break;
        case PetalID::kPeas: Petals::Peas(ctx, r); break;
        case PetalID::kSand: Petals::Sand(ctx, r); break;
        case PetalID::kPincer: Petals::Pincer(ctx, r); break;
        case PetalID::kDahlia: Petals::Dahlia(ctx, r); break;
        case PetalID::kTriplet: Petals::Triplet(ctx, r); break;
        case PetalID::kAntEgg: Petals::AntEgg(ctx, r); break;
        case PetalID::kPollen: Petals::Pollen(ctx, r); break;
        case PetalID::kPoisonPeas: Petals::PoisonPeas(ctx, r); break;
        case PetalID::kBeetleEgg: Petals::BeetleEgg(ctx, r); break;
        case PetalID::kAzalea: Petals::Azalea(ctx, r); break;
        case PetalID::kStick: Petals::Stick(ctx, r); break;
        case PetalID::kTringer: Petals::Tringer(ctx, r); break;
        case PetalID::kTriweb: Petals::Triweb(ctx, r); break;
        case PetalID::kAntennae: Petals::Antennae(ctx, r); break;
        case PetalID::kTricac: Petals::Tricac(ctx, r); break;
        case PetalID::kHeaviest: Petals::Heaviest(ctx, r); break;
        case PetalID::kThirdEye: Petals::ThirdEye(ctx, r); break;
        case PetalID::kPoisonCactus: Petals::PoisonCactus(ctx, r); break;
        case PetalID::kSalt: Petals::Salt(ctx, r); break;
        case PetalID::kUniqueBasic: Petals::UniqueBasic(ctx, r); break;
        case PetalID::kSquare: Petals::Square(ctx, r); break;
        case PetalID::kLotus: Petals::Lotus(ctx, r); break;
        case PetalID::kCutter: Petals::Cutter(ctx, r); break;
        case PetalID::kYinYang: Petals::YinYang(ctx, r); break;
        case PetalID::kYggdrasil: Petals::Yggdrasil(ctx, r); break;
        case PetalID::kRice: Petals::Rice(ctx, r); break;
        case PetalID::kBone: Petals::Bone(ctx, r); break;
        case PetalID::kYucca: Petals::Yucca(ctx, r); break;
        case PetalID::kCorn: Petals::Corn(ctx, r); break;
        case PetalID::kGoldenLeaf: Petals::GoldenLeaf(ctx, r); break;
        case PetalID::kLPeas: Petals::LPeas(ctx, r); break;
        case PetalID::kSoil: Petals::Soil(ctx, r); break;
        case PetalID::kTriFaster: Petals::TriFaster(ctx, r); break;
        default:
            assert(id < PetalID::kNumPetals);
            assert(!"didn't cover petal render");
            break;
    }
}


void draw_static_petal(PetalID::T id, Renderer &ctx) {
    struct PetalData const &data = PETAL_DATA[id];
    uint32_t count = data.count;
    if (count == 0) count = 1;
    for (uint32_t i = 0; i < count; ++i) {
        RenderContext context(&ctx);
        float rad = 10;
        if (data.attributes.clump_radius_icon != 0)
            rad = data.attributes.clump_radius_icon;
        else if (data.attributes.clump_radius != 0)
            rad = data.attributes.clump_radius;
        ctx.rotate(i * 2 * M_PI / data.count);
        if (data.count > 1) ctx.translate(rad, 0);
        ctx.rotate(data.attributes.icon_angle);
        draw_static_petal_single(id, ctx);
    }
}

void draw_loadout_background(Renderer &ctx, uint8_t id, float reload) {
    RenderContext c(&ctx);
    ctx.set_fill(Renderer::HSV(RARITY_COLORS[PETAL_DATA[id].rarity], 0.8));
    ctx.round_line_join();
    ctx.round_line_cap();
    ctx.begin_path();
    ctx.round_rect(-30, -30, 60, 60, 3);
    ctx.fill();
    ctx.set_fill(RARITY_COLORS[PETAL_DATA[id].rarity]);
    ctx.begin_path();
    ctx.rect(-25, -25, 50, 50);
    ctx.fill();
    ctx.clip();
    if (reload < 1) {
        float rld =  1 - (float) reload;
        {
            rld = rld * rld * rld * (rld * (6.0f * rld - 15.0f) + 10.0f);
            RenderContext context(&ctx);
            ctx.set_fill(0x40000000);
            ctx.begin_path();
            ctx.move_to(0,0);
            ctx.partial_arc(0, 0, 90, -M_PI / 2 - rld * M_PI * 10, -M_PI / 2 - rld * M_PI * 8, 0);
            ctx.fill();
        }
    }
    ctx.translate(0, -5);
    {
        RenderContext r(&ctx);
        ctx.scale(0.833);
        if (PETAL_DATA[id].radius > 20) ctx.scale(20 / PETAL_DATA[id].radius);
        draw_static_petal(id, ctx);
    }
    float text_width = 12 * Renderer::get_ascii_text_size(PETAL_DATA[id].name);
    if (text_width < 50) text_width = 12;
    else text_width = 12 * 50 / text_width;
    ctx.translate(0, 20);
    ctx.draw_text(PETAL_DATA[id].name, { .size = text_width });
}