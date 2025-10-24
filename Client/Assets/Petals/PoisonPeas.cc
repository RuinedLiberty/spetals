#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void PoisonPeas(Renderer &ctx, float r) {
    ctx.set_fill(0xffce76db);
    ctx.set_stroke(0xffa760b1);
    ctx.set_line_width(3);
    ctx.begin_path();
    ctx.arc(0,0,r);
    ctx.fill();
    ctx.stroke();
}
}
