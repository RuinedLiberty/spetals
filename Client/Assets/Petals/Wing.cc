#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Wing(Renderer &ctx, float r) {
    (void)r;
    ctx.begin_path();
    ctx.partial_arc(0,0,15,-1.5707963267948966,1.5707963267948966,0);
    ctx.qcurve_to(10,0,0,-15);
    ctx.set_fill(0xffffffff);
    ctx.fill();
    ctx.set_stroke(0xffcfcfcf);
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.stroke();
}
}
