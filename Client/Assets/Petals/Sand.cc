#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Sand(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xffe0c85c);
    ctx.set_stroke(0xffb5a24b);
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(7,0);
    ctx.line_to(3.499999761581421,6.062178134918213);
    ctx.line_to(-3.500000476837158,6.062177658081055);
    ctx.line_to(-7,-6.119594218034763e-7);
    ctx.line_to(-3.4999992847442627,-6.062178134918213);
    ctx.line_to(3.4999992847442627,-6.062178134918213);
    ctx.close_path();
    ctx.fill();
    ctx.stroke();
}
}
