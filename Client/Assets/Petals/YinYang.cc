#include <Client/Assets/Petals/Petals.hh>
#include <cmath>

namespace Petals {
void YinYang(Renderer &ctx, float r) {
    ctx.set_line_width(3);
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xffcfcfcf);
    ctx.begin_path();
    ctx.partial_arc(0,0,r,M_PI/2,3*M_PI/2,0);
    ctx.partial_arc(0,-r/2,r/2,-M_PI/2,M_PI/2,0);
    ctx.partial_arc(0,r/2,r/2,-M_PI/2,M_PI/2,1);
    ctx.fill();
    ctx.stroke();
    ctx.set_fill(0xff333333);
    ctx.set_stroke(0xff292929);
    ctx.begin_path();
    ctx.partial_arc(0,0,r,-M_PI/2,M_PI/2,0);
    ctx.partial_arc(0,r/2,r/2,M_PI/2,3*M_PI/2,0);
    ctx.partial_arc(0,-r/2,r/2,M_PI/2,3*M_PI/2,1);
    ctx.fill();
    ctx.stroke();
    ctx.set_stroke(0xffcfcfcf);
    ctx.begin_path();
    ctx.partial_arc(0,0,r,M_PI,3*M_PI/2,0);
    ctx.partial_arc(0,-r/2,r/2,-M_PI/2,M_PI/2,0);
    ctx.stroke();
}
}
