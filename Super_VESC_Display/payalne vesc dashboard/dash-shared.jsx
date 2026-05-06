// dash-shared.jsx — общие данные и примитивы для всех вариантов VESC дашборда
// Все варианты используют один источник симулированных данных, чтобы
// сравнения были корректными.

const DASH_DATA = {
  speed: 32,           // km/h
  speedMax: 60,
  speedAvg: 24,
  speedTopRide: 47,
  battery: 68,         // %
  voltage: 54.2,       // V (14S Li-ion ~ 58.8V max, ~42V min)
  voltageMin: 42.0,
  voltageMax: 58.8,
  motorCurrent: 18.4,  // A
  motorCurrentMax: 80,
  power: 998,          // W
  powerMax: 4500,
  motorTemp: 62,       // C
  controllerTemp: 48,  // C
  rpm: 2840,
  rpmMax: 6000,
  odometer: 4128.7,    // km total
  trip: 14.2,          // km this ride
  range: 38,           // km estimated remaining
  rideTime: '00:42:18',
  whPerKm: 9.4,
  mode: 2,             // 1/2/3
  modeName: 'SPORT',
  bluetooth: true,
  gps: true,
  signal: 4,
};

// Hook for live-ish updates so dashboards feel alive when displayed.
// Every artboard calls this; values jitter slightly around the base so
// displays don't look frozen, but core metrics stay readable.
function useLiveData(seed = 0) {
  const [t, setT] = React.useState(0);
  React.useEffect(() => {
    let raf;
    const start = performance.now();
    const tick = (now) => {
      setT((now - start) / 1000);
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, []);

  // smooth-ish jitter
  const j = (amp, freq, phase) =>
    Math.sin(t * freq + phase + seed) * amp;

  return {
    ...DASH_DATA,
    speed: Math.max(0, Math.round(DASH_DATA.speed + j(2.2, 0.6, 0))),
    rpm: Math.max(0, Math.round(DASH_DATA.rpm + j(180, 0.55, 1.2))),
    power: Math.max(0, Math.round(DASH_DATA.power + j(140, 0.7, 2.4))),
    motorCurrent: +(DASH_DATA.motorCurrent + j(2.4, 0.8, 0.3)).toFixed(1),
    voltage: +(DASH_DATA.voltage + j(0.18, 0.3, 1.7)).toFixed(1),
    motorTemp: Math.round(DASH_DATA.motorTemp + j(1.4, 0.12, 0.9)),
    controllerTemp: Math.round(DASH_DATA.controllerTemp + j(1.0, 0.1, 2.1)),
    t,
  };
}

// Pad number with leading zeroes
function pad(n, len = 2) {
  return String(Math.floor(n)).padStart(len, '0');
}

// Helper: clamped percent
const pct = (v, min, max) =>
  Math.max(0, Math.min(1, (v - min) / (max - min)));

// ── Shared widgets (built to be LVGL-implementable: flat fills, simple shapes) ──

// Horizontal bar
function HBar({ value, max, height = 10, color, bg = '#161b1e', radius = 2, segments = 0 }) {
  const w = Math.max(0, Math.min(1, value / max)) * 100;
  return (
    <div style={{
      position: 'relative', width: '100%', height,
      background: bg, borderRadius: radius, overflow: 'hidden'
    }}>
      <div style={{
        width: w + '%', height: '100%',
        background: color || 'var(--vesc-accent)',
        transition: 'width 0.3s ease',
      }} />
      {segments > 0 && Array.from({ length: segments - 1 }).map((_, i) => (
        <div key={i} style={{
          position: 'absolute', left: ((i + 1) / segments * 100) + '%',
          top: 0, bottom: 0, width: 2, background: bg
        }} />
      ))}
    </div>
  );
}

// Top status bar shared by some variants
function StatusBar({ data, accent = 'var(--vesc-accent)', style = {} }) {
  return (
    <div style={{
      position: 'absolute', top: 0, left: 0, right: 0, height: 32,
      display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      padding: '0 16px',
      borderBottom: '1px solid var(--vesc-line)',
      fontFamily: "'JetBrains Mono', monospace",
      fontSize: 11,
      color: 'var(--vesc-text-dim)',
      letterSpacing: '0.06em',
      textTransform: 'uppercase',
      background: 'var(--vesc-bg-0)',
      ...style,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
        <span style={{ color: accent, fontWeight: 700, letterSpacing: '0.16em' }}>VESC</span>
        <span>{data.modeName}</span>
        <span>{data.rideTime}</span>
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
        {data.bluetooth && <span style={{ color: accent }}>BT</span>}
        {data.gps && <span>GPS</span>}
        <span>{data.controllerTemp}°C</span>
      </div>
    </div>
  );
}

// Mini stat block: small label + big number + unit
function Stat({ label, value, unit, big = 30, accent = 'var(--vesc-text)', align = 'left' }) {
  return (
    <div style={{ textAlign: align }}>
      <div style={{
        fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
        letterSpacing: '0.16em', textTransform: 'uppercase',
        color: 'var(--vesc-text-dim)', marginBottom: 2,
      }}>{label}</div>
      <div style={{
        fontFamily: "'Barlow Condensed', sans-serif",
        fontWeight: 700, fontSize: big, lineHeight: 1, color: accent,
        letterSpacing: '-0.01em',
      }} className="num">
        {value}
        {unit && <span style={{
          fontSize: big * 0.42, color: 'var(--vesc-text-dim)',
          fontWeight: 500, marginLeft: 4,
        }}>{unit}</span>}
      </div>
    </div>
  );
}

// Tick marks for dial-style gauges
function TickMarks({ count = 11, length = 8, color = 'var(--vesc-text-faint)', strong = 0 }) {
  return (
    <>
      {Array.from({ length: count }).map((_, i) => {
        const a = (i / (count - 1)) * 240 - 210; // start from 7-o'clock
        const isStrong = strong > 0 && i % strong === 0;
        return (
          <div key={i} style={{
            position: 'absolute', left: '50%', top: '50%',
            width: 2, height: isStrong ? length + 4 : length,
            background: color,
            transformOrigin: 'center calc(100% + 130px)',
            transform: `translate(-50%, -100%) rotate(${a}deg) translateY(-130px)`,
          }} />
        );
      })}
    </>
  );
}

window.DASH_DATA = DASH_DATA;
window.useLiveData = useLiveData;
window.pad = pad;
window.pct = pct;
window.HBar = HBar;
window.StatusBar = StatusBar;
window.Stat = Stat;
window.TickMarks = TickMarks;
