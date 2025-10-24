#include <Client/Assets/Petals/Petals.hh>

namespace Petals {
void Salt(Renderer &ctx, float r) {
    (void)r;
    ctx.set_fill(0xffffffff);
    ctx.set_stroke(0xffcfcfcf);
    ctx.set_line_width(3);
    ctx.round_line_cap();
    ctx.round_line_join();
    ctx.begin_path();
    ctx.move_to(10.404077529907227,0);
    ctx.line_to(6.643442630767822,8.721502304077148);
    ctx.line_to(-2.6667866706848145,11.25547981262207);
    ctx.line_to(-10.940428733825684,4.95847225189209);
    ctx.line_to(-11.341578483581543,-5.432167053222656);
    ctx.line_to(-2.4972469806671143,-11.472168922424316);
    ctx.line_to(7.798409461975098,-9.584606170654297);
    ctx.line_to(10.404077529907227,0);
    ctx.fill();
    ctx.stroke();
}
}
