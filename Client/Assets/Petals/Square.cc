#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Square(Renderer &ctx, float r) {
    ctx.set_fill(0xffffe869);
    ctx.set_stroke(0xffcfbc55);
    ctx.set_line_width(0.15f*r);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.rect(-r * 0.707f, -r * 0.707f, r * 1.414f, r * 1.414f);
    ctx.fill();
    ctx.stroke();
}
}
