#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Rock(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xff777777);
    ctx.set_stroke(Renderer::HSV(0xff777777, 0.8));
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(12.138091087341309,0);
    ctx.line_to(3.8414306640625,12.377452850341797);
    ctx.line_to(-11.311542510986328,7.916932582855225);
    ctx.line_to(-11.461170196533203,-7.836822032928467);
    ctx.line_to(4.538298606872559,-13.891617774963379);
    ctx.line_to(12.138091087341309,0);
    ctx.close_path();
    ctx.fill();
    ctx.stroke();
}
}
