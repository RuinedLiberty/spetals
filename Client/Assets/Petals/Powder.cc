#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Powder(Renderer &ctx, float r) {
    (void)r; // source asset is size-locked; caller scales externally if needed
    RenderContext S(&ctx);

    ctx.scale(1.0037914163125217f, 1.0037914163125217f);
    ctx.scale(3.779527559055118f, 3.779527559055118f);
    ctx.translate(-99.788881f, -137.75018f);
    // Center the Inkscape export around (0,0) without altering the path data
    // Average cluster center after the translate is approximately (5.31712, 5.24182)
    ctx.translate(-5.31712f, -5.24182f);


    // Appearance
    ctx.set_fill(0xffeeeeee);
    ctx.set_stroke(0x00000000);
    ctx.set_line_width(0.410389f); 

    constexpr float R = 2.34506f;

    const struct { float x, y; } C[] = {
        {104.77541f, 140.81998f},
        {107.27518f, 147.21755f},
        {105.20509f, 145.31538f},
        {104.16465f, 144.16465f},
        {103.31590f, 142.56043f},
        {103.64284f, 143.67956f},
        {108.77753f, 140.42035f},
        {107.53046f, 144.93741f},
        {103.07190f, 140.09523f},
        {108.49147f, 143.50803f},
        {102.66845f, 144.47861f},
        {105.35866f, 142.50010f},
        {106.01314f, 141.26566f},
        {102.13384f, 143.04294f},
        {104.16971f, 140.87044f},
    };

    for (auto const &c : C) {
        ctx.begin_path();
        ctx.ellipse(c.x, c.y, R, R);
        ctx.fill();
    }
}
}
