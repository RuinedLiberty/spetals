#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Heaviest(Renderer &ctx, float r) {
    (void)r;
    ctx.begin_path();
    ctx.arc(0,0,16);
    ctx.set_fill(0xff333333);
    ctx.fill();
    ctx.set_stroke(0xff292929);
    ctx.set_line_width(3);
    ctx.stroke();
    ctx.begin_path();
    ctx.arc(6,-6,4.6);
    ctx.set_fill(0xffcccccc);
    ctx.fill();
}
}
