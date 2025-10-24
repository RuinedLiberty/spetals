#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Bone(Renderer &ctx, float r) {
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xffcfcfcf);
    ctx.set_line_width(5);
    ctx.scale(r / 12);
    ctx.begin_path();
    ctx.move_to(-10,-4);
    ctx.qcurve_to(0,0,10,-4);
    ctx.bcurve_to(14,-10,20,-2,14,0);
    ctx.bcurve_to(20,2,14,10,10,4);
    ctx.qcurve_to(0,0,-10,4);
    ctx.bcurve_to(-14,10,-20,2,-14,0);
    ctx.bcurve_to(-20,-2,-14,-10,-10,-4);
    ctx.stroke();
    ctx.fill();
}
}
