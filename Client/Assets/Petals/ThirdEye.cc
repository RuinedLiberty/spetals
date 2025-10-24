#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void ThirdEye(Renderer &ctx, float r) {
    (void)r;
    ctx.scale(0.5f);
    ctx.set_fill(0xff111111);
    ctx.begin_path();
    ctx.move_to(0,-10);
    ctx.qcurve_to(8,0,0,10);
    ctx.qcurve_to(-8,0,0,-10);
    ctx.fill();
    ctx.set_fill(0xffeeeeee);
    ctx.begin_path();
    ctx.arc(0, 0, 5);
    ctx.fill();
    ctx.set_stroke(0xff111111);
    ctx.set_line_width(1.5);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(0,-10);
    ctx.qcurve_to(8,0,0,10);
    ctx.qcurve_to(-8,0,0,-10);
    ctx.stroke();
}
}
