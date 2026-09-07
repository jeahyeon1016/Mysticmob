// 수평 12mm 육각 소켓 + 위쪽 M3 고정나사
$fn=48; cut_y=-12; head_y0=-12; head_len=20; outer_r=9;
socket_r=12.2/sqrt(3); socket_depth=6; m3=3.4;
module yc(r,y,l,n=48){translate([0,y,0]) rotate([-90,0,0]) cylinder(r=r,h=l,$fn=n);}
// 위쪽에 꼭짓점이 아니라 평평한 육각면이 오도록 배치
module yh(r,y,l){translate([0,y,0]) rotate([-90,0,0]) cylinder(r=r,h=l,$fn=6);}
module head(){difference(){yc(outer_r,head_y0,head_len,8); yh(socket_r,head_y0+head_len-socket_depth,socket_depth+.2); translate([0,head_y0+head_len-4,-1]) cylinder(d=m3,h=22);}}
difference(){import("end.stl",convexity=10); translate([-30,cut_y,-20]) cube([60,40,40]);}
head();
