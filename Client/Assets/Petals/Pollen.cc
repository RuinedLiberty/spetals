#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Pollen(Renderer &ctx, float r) {
    ctx.set_fill(0xffffe763);
    ctx.set_stroke(0xffcfbb50);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
