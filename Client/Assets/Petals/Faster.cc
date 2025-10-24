#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Faster(Renderer &ctx, float r) {
    ctx.set_fill(0xfffeffc9);
    ctx.set_stroke(0xffcecfa3);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
