// DICEBAG — display: scrolling X1/X2/X3/Y traces and t1/t2/t3 gate lanes. kalabash.
mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
autowatch = 1;

var N = 96;
var hist = [];
for (var i = 0; i < N; ++i) hist.push([0, 0, 0, 0.5, 0.5, 0.5, 0.5]);
var dv = [0, 0], len = 8;
var cols = [[1.0, 0.62, 0.25], [0.45, 0.85, 1.0], [0.85, 0.5, 1.0], [0.6, 0.95, 0.55]];

function frame(t1, t2, t3, x1, x2, x3, y, dvt, dvx, l) {
  hist.shift();
  hist.push([t1, t2, t3, x1, x2, x3, y]);
  dv = [dvt, dvx]; len = l;
  mgraphics.redraw();
}

function paint() {
  var w = box.rect[2] - box.rect[0], h = box.rect[3] - box.rect[1];
  with (mgraphics) {
    set_source_rgba(0.07, 0.07, 0.08, 1); rectangle(0, 0, w, h); fill();
    var laneH = 7, top = 4;
    // gate lanes
    for (var g = 0; g < 3; ++g) {
      for (var i = 0; i < N; ++i) {
        if (hist[i][g]) {
          var c = cols[g];
          set_source_rgba(c[0], c[1], c[2], 0.9);
          rectangle(i * w / N, top + g * (laneH + 2), w / N + 0.5, laneH); fill();
        }
      }
    }
    // traces
    var y0 = top + 3 * (laneH + 2) + 4, th = h - y0 - 12;
    set_source_rgba(1, 1, 1, 0.06); rectangle(0, y0, w, th); fill();
    for (var k = 0; k < 4; ++k) {
      var c2 = cols[k];
      set_source_rgba(c2[0], c2[1], c2[2], k === 3 ? 0.6 : 0.95);
      set_line_width(k === 3 ? 1 : 1.5);
      for (var j = 0; j < N; ++j) {
        var px = j * w / (N - 1), py = y0 + th - hist[j][3 + k] * th;
        if (j === 0) move_to(px, py); else line_to(px, py);
      }
      stroke();
    }
    // deja vu indicator
    select_font_face("Arial"); set_font_size(9);
    set_source_rgba(0.85, 0.85, 0.85, 0.8);
    move_to(3, h - 3);
    text_path("DV t " + Math.round(dv[0] * 100) + "  x " + Math.round(dv[1] * 100) + "  len " + len);
    fill();
  }
}
