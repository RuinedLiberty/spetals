#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Bubble(Renderer &ctx, float r) {
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.set_stroke(0xb2ffffff);
    ctx.set_line_width(3);
    ctx.stroke();
    ctx.begin_path();
    ctx.arc(0,0,r-1.5f);
    ctx.set_fill(0x59ffffff);
    ctx.fill();
    ctx.begin_path();
    ctx.arc(r/3,-r/3,r/4);
    ctx.set_fill(0x59ffffff);
    ctx.fill();
}
}
