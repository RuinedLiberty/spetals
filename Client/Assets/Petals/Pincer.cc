#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Pincer(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xff333333);
    ctx.set_stroke(0xff292929);
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(10,5);
    ctx.qcurve_to(4,-14,-10,5);
    ctx.qcurve_to(4,0,10,5);
    ctx.fill();
    ctx.stroke();
}
}
