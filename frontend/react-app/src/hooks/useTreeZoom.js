import { useCallback, useEffect, useLayoutEffect, useRef, useState } from "react";

const MIN_SCALE = 0.15;
const MAX_SCALE = 2.5;
const STEP = 0.15;

/** Keeps a scrollable tree viewport auto-fitted to its container on first
 * render and whenever the tree's own size changes, with zoom in/out/fit
 * controls layered on top for manual adjustment.
 *
 * Tabs in this app stay mounted and are only toggled via the `hidden`
 * attribute (so state like walkthrough playback position survives a tab
 * switch), which means an inactive tab's tree container is `display:none`
 * -- 0x0 -- at the moment this hook's effect first runs. Fitting against a
 * 0x0 box is a no-op, so without watching for the container's real size
 * showing up later, a tree opened on a tab that wasn't active on load would
 * never auto-fit and would just render at 100%, overflowing. The
 * ResizeObserver below fires once the container actually gets a layout box
 * (tab switched to) and re-fits then. */
export function useTreeZoom(width, height) {
  const containerRef = useRef(null);
  const [scale, setScale] = useState(1);

  const fit = useCallback(() => {
    const el = containerRef.current;
    if (!el || !width || !height) return;
    const pad = 28;
    const availW = el.clientWidth - pad;
    const availH = el.clientHeight - pad;
    if (availW <= 0 || availH <= 0) return;
    const s = Math.min(availW / width, availH / height, 1);
    setScale(s > 0 ? s : 1);
  }, [width, height]);

  useLayoutEffect(() => {
    fit();
  }, [fit]);

  useEffect(() => {
    const el = containerRef.current;
    if (!el || typeof ResizeObserver === "undefined") return undefined;
    const ro = new ResizeObserver(() => fit());
    ro.observe(el);
    return () => ro.disconnect();
  }, [fit]);

  const zoomIn = useCallback(() => setScale((s) => Math.min(MAX_SCALE, s + STEP)), []);
  const zoomOut = useCallback(() => setScale((s) => Math.max(MIN_SCALE, s - STEP)), []);

  return { containerRef, scale, fit, zoomIn, zoomOut };
}
