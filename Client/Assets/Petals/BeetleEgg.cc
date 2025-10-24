#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void BeetleEgg(Renderer &ctx, float r) {
    ctx.begin_path();
    ctx.ellipse(0,0,r * 0.85f, r * 1.15f);
    ctx.set_fill(0xfffff0b8);
    ctx.fill();
    ctx.set_stroke(0xffcfc295);
    ctx.set_line_width(3);
    ctx.stroke();
}
}
