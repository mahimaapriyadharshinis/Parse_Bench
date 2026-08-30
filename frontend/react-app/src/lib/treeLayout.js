/** Simple recursive tree layout: leaves get sequential x slots, internal
 * nodes center over their children; y is proportional to depth. */
export function layoutTree(root) {
  const XGAP = 78;
  const YGAP = 62;
  const PAD = 30;
  let leafCounter = 0;
  const positioned = [];

  function assign(n, depth) {
    if (!n.children.length) {
      n._x = leafCounter * XGAP;
      leafCounter++;
    } else {
      for (const c of n.children) assign(c, depth + 1);
      const xs = n.children.map((c) => c._x);
      n._x = (Math.min(...xs) + Math.max(...xs)) / 2;
    }
    n._y = depth * YGAP;
    n._leaf = !n.children.length;
    positioned.push(n);
  }
  assign(root, 0);

  const edges = [];
  (function collect(n) {
    for (const c of n.children) {
      edges.push([n, c]);
      collect(c);
    }
  })(root);

  const maxX = Math.max(0, ...positioned.map((n) => n._x));
  const maxY = Math.max(0, ...positioned.map((n) => n._y));
  const width = maxX + PAD * 2 + 40;
  const height = maxY + PAD * 2 + 26;
  const ox = PAD + 20;
  const oy = PAD + 13;
  for (const n of positioned) {
    n._x += ox;
    n._y += oy;
  }
  return { nodes: positioned, edges, width, height };
}
