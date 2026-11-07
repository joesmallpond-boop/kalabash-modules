// DICEBAG — modulation mapper (one instance per output: X1, X2, X3, Y)
// Click MAP, then click any parameter in Live (devices, rack macros, mixer...).
// The output then drives that parameter through live.remote~ between MIN and MAX.
// kalabash.
autowatch = 1;
inlets = 3;    // 0: messages (mod, min, max, restore, init) | 1: MAP button | 2: UNMAP button
outlets = 4;   // 0: live.remote~ | 1: label (comment) | 2: pattr (persistent path) | 3: MAP button LED

var idx = jsarguments.length > 1 ? parseInt(jsarguments[1], 10) : 0;
var names = ["X1", "X2", "X3", "Y"];
var ready = false, pendingPath = "", mapping = false, mappingFromId = -1;
var bound = false, boundPath = "", pmin = 0, pmax = 1, lo = 0, hi = 1, last = -1e9;
var selObserver = null, depthOn = 1;

function label(s) { outlet(1, "set", s); }

function init() {
  ready = true;
  if (pendingPath.length) { var p = pendingPath; pendingPath = ""; bindPath(p); }
  else if (!bound) label(names[idx] + "  —  not mapped");
}

function msg_int(v) {
  if (inlet === 1 && v) toggleMap();
  else if (inlet === 2 && v) unmap();
}

function toggleMap() {
  if (!ready) return;
  if (mapping) { mapping = false; outlet(3, 0); label(bound ? currentLabel : names[idx] + "  —  not mapped"); return; }
  mapping = true;
  outlet(3, 1);
  label("click a parameter in Live...");
  var view = new LiveAPI("live_set view");
  var cur = view.get("selected_parameter");
  mappingFromId = cur && cur.length ? parseInt(cur[cur.length - 1], 10) : -1;
  if (!selObserver) {
    selObserver = new LiveAPI(onSelected, "live_set view");
    selObserver.property = "selected_parameter";
  }
}

function onSelected(args) {
  if (!mapping || !args || args.length < 1) return;
  var id = parseInt(args[args.length - 1], 10);
  if (!id || id === mappingFromId) return;
  var a = new LiveAPI("id " + id);
  if (!a || parseInt(a.id, 10) === 0) return;
  var path = a.unquotedpath;
  var me = new LiveAPI("this_device").unquotedpath;
  if (path.indexOf(me + " ") === 0) { label("can't map to itself — pick another"); return; }
  mapping = false;
  outlet(3, 0);
  bindApi(a, path);
}

var currentLabel = "";
function bindApi(a, path) {
  pmin = parseFloat(a.get("min"));
  pmax = parseFloat(a.get("max"));
  if (!(pmax > pmin)) { pmin = 0; pmax = 1; }
  outlet(0, "id", parseInt(a.id, 10));
  bound = true;
  boundPath = path;
  last = -1e9;
  var pname = String(a.get("name"));
  var devPath = path.replace(/ parameters \d+$/, "");
  var dname = "";
  if (devPath !== path) {
    var d = new LiveAPI(devPath);
    if (d && parseInt(d.id, 10) !== 0) dname = String(d.get("name"));
  }
  currentLabel = names[idx] + " > " + (dname.length ? dname + " : " : "") + pname;
  label(currentLabel);
  outlet(2, ["set"].concat(path.split(" ")));
}

function bindPath(path) {
  var a = new LiveAPI(path);
  if (!a || parseInt(a.id, 10) === 0) { label(names[idx] + "  —  target missing"); outlet(2, "set", "none"); return; }
  bindApi(a, path);
}

function unmap() {
  mapping = false;
  outlet(3, 0);
  outlet(0, "id", 0);
  bound = false;
  boundPath = "";
  label(names[idx] + "  —  not mapped");
  outlet(2, "set", "none");
}

function restore() {
  var parts = arrayfromargs(arguments);
  var p = parts.join(" ");
  if (!p.length || p === "none" || p === "0") return;
  if (ready) bindPath(p); else pendingPath = p;
}

function min(v) { lo = v; last = -1e9; }
function max(v) { hi = v; last = -1e9; }
function active(v) { depthOn = v ? 1 : 0; }

function mod() {
  if (!bound || !depthOn) return;
  var x = arguments[idx];
  if (x === undefined) return;
  var n = lo + (hi - lo) * x;
  var val = pmin + (pmax - pmin) * n;
  if (Math.abs(val - last) < (pmax - pmin) * 0.0005) return;
  last = val;
  outlet(0, val);
}
mod.immediate = 1;
