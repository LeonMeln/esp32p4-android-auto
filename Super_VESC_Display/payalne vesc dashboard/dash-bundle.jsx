// ===== dash-shared.jsx =====
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


// ===== dash-v1-cockpit.jsx =====
// V1 — COCKPIT
// Классический racing-layout: огромная скорость по центру, батарея слева,
// мощность справа, статус-бар сверху, мини-метрики снизу.
// Всё на плоских заливках — легко реализовать в LVGL (lv_label, lv_bar, lv_arc).

function DashCockpit() {
  const d = useLiveData(1);
  const speedPct = pct(d.speed, 0, d.speedMax);
  const batColor = d.battery > 50 ? 'var(--vesc-accent)' : d.battery > 20 ? 'var(--vesc-warn)' : 'var(--vesc-danger)';

  return (
    <div className="dash">
      <StatusBar data={d} />

      {/* Main grid: battery | speed | power */}
      <div style={{
        position: 'absolute', top: 32, left: 0, right: 0, bottom: 80,
        display: 'grid', gridTemplateColumns: '180px 1fr 180px',
      }}>
        {/* LEFT — battery vertical */}
        <div style={{
          padding: '24px 16px 16px 20px',
          borderRight: '1px solid var(--vesc-line)',
          display: 'flex', flexDirection: 'column',
        }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
            letterSpacing: '0.18em', color: 'var(--vesc-text-dim)',
            textTransform: 'uppercase', marginBottom: 10,
          }}>BATTERY</div>

          {/* big % */}
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 64, lineHeight: 0.9, color: batColor,
          }} className="num">
            {d.battery}<span style={{ fontSize: 24, color: 'var(--vesc-text-dim)', fontWeight: 500, marginLeft: 4 }}>%</span>
          </div>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 12,
            color: 'var(--vesc-text-dim)', marginTop: 4,
          }}>{d.voltage.toFixed(1)} V</div>

          {/* segmented vertical bar */}
          <div style={{
            marginTop: 18, flex: 1,
            display: 'flex', flexDirection: 'column', justifyContent: 'flex-end',
            gap: 3,
          }}>
            {Array.from({ length: 14 }).map((_, i) => {
              const filled = (13 - i) / 14 < d.battery / 100;
              return (
                <div key={i} style={{
                  height: 12, background: filled ? batColor : 'var(--vesc-bg-2)',
                  borderRadius: 1,
                  opacity: filled ? 1 : 1,
                }} />
              );
            })}
          </div>

          <div style={{
            display: 'flex', justifyContent: 'space-between',
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            color: 'var(--vesc-text-dim)', marginTop: 10,
            letterSpacing: '0.06em',
          }}>
            <span>RANGE</span>
            <span style={{ color: 'var(--vesc-text)' }}>{d.range} KM</span>
          </div>
        </div>

        {/* CENTER — Speed */}
        <div style={{
          display: 'flex', flexDirection: 'column',
          alignItems: 'center', justifyContent: 'center',
          position: 'relative',
        }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            letterSpacing: '0.3em', color: 'var(--vesc-text-dim)',
            marginBottom: 4,
          }}>SPEED · KM/H</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 220, lineHeight: 0.85, color: 'var(--vesc-text)',
            letterSpacing: '-0.04em',
            textShadow: '0 0 40px rgba(182,255,46,0.08)',
          }} className="num">
            {pad(d.speed)}
          </div>

          {/* speed bar */}
          <div style={{ width: 320, marginTop: 14 }}>
            <HBar value={d.speed} max={d.speedMax} height={6} color="var(--vesc-accent)" segments={12} />
          </div>
          <div style={{
            display: 'flex', justifyContent: 'space-between', width: 320,
            fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
            color: 'var(--vesc-text-faint)', marginTop: 6,
          }}>
            <span>0</span><span>MAX {d.speedMax}</span>
          </div>
        </div>

        {/* RIGHT — Power */}
        <div style={{
          padding: '24px 20px 16px 16px',
          borderLeft: '1px solid var(--vesc-line)',
          display: 'flex', flexDirection: 'column', alignItems: 'flex-end',
        }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
            letterSpacing: '0.18em', color: 'var(--vesc-text-dim)',
            textTransform: 'uppercase', marginBottom: 10,
          }}>POWER</div>

          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 64, lineHeight: 0.9, color: 'var(--vesc-accent)',
          }} className="num">
            {(d.power / 1000).toFixed(2)}
            <span style={{ fontSize: 24, color: 'var(--vesc-text-dim)', fontWeight: 500, marginLeft: 4 }}>kW</span>
          </div>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 12,
            color: 'var(--vesc-text-dim)', marginTop: 4,
          }}>{d.motorCurrent.toFixed(1)} A · {d.rpm} RPM</div>

          {/* horizontal power bar with segments */}
          <div style={{
            marginTop: 18, width: '100%', flex: 1,
            display: 'flex', flexDirection: 'column', justifyContent: 'flex-end',
            gap: 3,
          }}>
            {Array.from({ length: 14 }).map((_, i) => {
              const filled = (13 - i) / 14 < d.power / d.powerMax;
              return (
                <div key={i} style={{
                  height: 12, background: filled ? 'var(--vesc-accent)' : 'var(--vesc-bg-2)',
                  borderRadius: 1,
                }} />
              );
            })}
          </div>

          <div style={{
            display: 'flex', justifyContent: 'space-between', width: '100%',
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            color: 'var(--vesc-text-dim)', marginTop: 10,
            letterSpacing: '0.06em',
          }}>
            <span>MAX</span>
            <span style={{ color: 'var(--vesc-text)' }}>{(d.powerMax / 1000).toFixed(1)} KW</span>
          </div>
        </div>
      </div>

      {/* Bottom strip with mini metrics */}
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 0, height: 80,
        display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)',
        borderTop: '1px solid var(--vesc-line)',
        background: 'var(--vesc-bg-1)',
      }}>
        {[
          { label: 'TRIP', value: d.trip.toFixed(1), unit: 'KM' },
          { label: 'ODO', value: Math.floor(d.odometer), unit: 'KM' },
          { label: 'M·TEMP', value: d.motorTemp, unit: '°C' },
          { label: 'C·TEMP', value: d.controllerTemp, unit: '°C' },
          { label: 'AVG', value: d.speedAvg, unit: 'KM/H' },
        ].map((s, i) => (
          <div key={i} style={{
            display: 'flex', flexDirection: 'column', justifyContent: 'center',
            alignItems: 'center',
            borderRight: i < 4 ? '1px solid var(--vesc-line)' : 'none',
          }}>
            <div style={{
              fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
              letterSpacing: '0.18em', color: 'var(--vesc-text-dim)',
              marginBottom: 4,
            }}>{s.label}</div>
            <div style={{
              fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
              fontSize: 32, lineHeight: 1, color: 'var(--vesc-text)',
            }} className="num">
              {s.value}
              <span style={{
                fontSize: 12, color: 'var(--vesc-text-dim)',
                fontWeight: 500, marginLeft: 3,
              }}>{s.unit}</span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

window.DashCockpit = DashCockpit;


// ===== dash-v2-minimal.jsx =====
// V2 — SPEEDOMETER GLYPH
// Скорость как огромный "глиф" слева. Справа — стопка вертикальных
// bar-индикаторов (батарея/мощность/мотор/ESC). Глаз ловит цифру в
// периферии, вертикальные полосы дают мгновенный "уровень" без чтения.

function DashGlyph() {
  const d = useLiveData(2);
  const batColor = d.battery > 50 ? 'var(--vesc-accent)' : d.battery > 20 ? 'var(--vesc-warn)' : 'var(--vesc-danger)';
  const motorColor = d.motorTemp > 80 ? 'var(--vesc-danger)' : d.motorTemp > 60 ? 'var(--vesc-warn)' : 'var(--vesc-accent)';
  const escColor = d.controllerTemp > 80 ? 'var(--vesc-danger)' : d.controllerTemp > 60 ? 'var(--vesc-warn)' : 'var(--vesc-accent)';

  const VColumn = ({ label, bar, max, color, big, sub }) => {
    const filled = pct(bar, 0, max);
    const SEG = 24;
    return (
      <div style={{
        flex: 1, display: 'flex', flexDirection: 'column',
        alignItems: 'center', gap: 8,
      }}>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
          color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
          textTransform: 'uppercase',
        }}>{label}</div>
        <div style={{
          width: 24, height: 200,
          background: 'var(--vesc-bg-2)',
          display: 'flex', flexDirection: 'column-reverse',
          gap: 1, padding: 2,
        }}>
          {Array.from({ length: SEG }).map((_, i) => (
            <div key={i} style={{
              flex: 1,
              background: i / SEG < filled ? color : 'transparent',
            }} />
          ))}
        </div>
        <div style={{
          fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
          fontSize: 28, lineHeight: 1, color: color,
        }} className="num">{big}</div>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
          color: 'var(--vesc-text-dim)', letterSpacing: '0.1em',
        }}>{sub}</div>
      </div>
    );
  };

  return (
    <div className="dash">
      <StatusBar data={d} />

      {/* LEFT — speed glyph */}
      <div style={{
        position: 'absolute', top: 32, left: 0, bottom: 64, width: 460,
        display: 'flex', flexDirection: 'column',
        justifyContent: 'center', padding: '0 32px',
      }}>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
          color: 'var(--vesc-text-dim)', letterSpacing: '0.4em',
        }}>SPEED · KM/H</div>
        <div style={{
          fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
          fontSize: 340, lineHeight: 0.78, color: 'var(--vesc-text)',
          letterSpacing: '-0.07em', marginTop: 4,
        }} className="num">
          {pad(d.speed)}
        </div>
        <div style={{ marginTop: 14 }}>
          <HBar value={d.speed} max={d.speedMax} height={6} color="var(--vesc-accent)" segments={20} />
        </div>
        <div style={{
          display: 'flex', justifyContent: 'space-between',
          fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
          color: 'var(--vesc-text-faint)', marginTop: 6,
        }}>
          <span>AVG {d.speedAvg}</span>
          <span>RPM {d.rpm}</span>
          <span>MAX {d.speedMax}</span>
        </div>
      </div>

      {/* RIGHT — vertical bars */}
      <div style={{
        position: 'absolute', top: 32, right: 0, bottom: 64, left: 460,
        borderLeft: '1px solid var(--vesc-line)',
        background: 'var(--vesc-bg-1)',
        padding: '20px 16px',
        display: 'flex', gap: 4,
      }}>
        <VColumn label="BAT" bar={d.battery} max={100} color={batColor}
          big={`${d.battery}%`} sub={`${d.voltage.toFixed(1)}V`} />
        <VColumn label="PWR" bar={d.power} max={d.powerMax} color="var(--vesc-accent)"
          big={(d.power / 1000).toFixed(1)} sub="kW" />
        <VColumn label="MOT" bar={d.motorTemp} max={100} color={motorColor}
          big={`${d.motorTemp}°`} sub="C" />
        <VColumn label="ESC" bar={d.controllerTemp} max={100} color={escColor}
          big={`${d.controllerTemp}°`} sub="C" />
      </div>

      {/* Bottom — ribbon stats */}
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 0, height: 64,
        background: 'var(--vesc-bg-0)',
        borderTop: '1px solid var(--vesc-line)',
        display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)',
      }}>
        {[
          { l: 'TRIP', v: d.trip.toFixed(1), u: 'KM' },
          { l: 'RANGE', v: d.range, u: 'KM' },
          { l: 'ODO', v: Math.floor(d.odometer).toLocaleString(), u: 'KM' },
          { l: 'TIME', v: d.rideTime, u: '' },
        ].map((s, i) => (
          <div key={i} style={{
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            gap: 10,
            borderRight: i < 3 ? '1px solid var(--vesc-line)' : 'none',
          }}>
            <span style={{
              fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
              color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
            }}>{s.l}</span>
            <span style={{
              fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
              fontSize: 26, color: 'var(--vesc-text)',
            }} className="num">
              {s.v}{s.u && <span style={{ fontSize: 12, color: 'var(--vesc-text-dim)', fontWeight: 500, marginLeft: 3 }}>{s.u}</span>}
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

window.DashGlyph = DashGlyph;


// ===== dash-v3-radial.jsx =====
// V3 — DATA GRID
// Кокпитный layout: всё разложено в строгую сетку как у пилота. Скорость
// крупная сверху-слева, рядом мощность. Снизу — таблица всех значений,
// каждая ячейка с лейблом и числом. Никакой иерархии "главный/второстепенный".

function DashGrid() {
  const d = useLiveData(3);
  const batColor = d.battery > 50 ? 'var(--vesc-accent)' : d.battery > 20 ? 'var(--vesc-warn)' : 'var(--vesc-danger)';

  const Cell = ({ label, value, unit, color = 'var(--vesc-text)', bar, barMax, barColor, alarm }) => (
    <div style={{
      padding: '10px 14px',
      borderRight: '1px solid var(--vesc-line)',
      borderBottom: '1px solid var(--vesc-line)',
      position: 'relative',
      background: alarm ? 'rgba(255,59,48,0.06)' : 'transparent',
    }}>
      <div style={{
        fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
        color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
        textTransform: 'uppercase',
      }}>{label}</div>
      <div style={{
        fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
        fontSize: 28, lineHeight: 1, color, marginTop: 2,
      }} className="num">
        {value}
        {unit && <span style={{
          fontSize: 12, color: 'var(--vesc-text-dim)', fontWeight: 500, marginLeft: 3,
        }}>{unit}</span>}
      </div>
      {bar !== undefined && (
        <div style={{ marginTop: 6 }}>
          <HBar value={bar} max={barMax} height={3} color={barColor || color} />
        </div>
      )}
    </div>
  );

  return (
    <div className="dash">
      {/* Top status row */}
      <div style={{
        position: 'absolute', top: 0, left: 0, right: 0, height: 28,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '0 14px',
        borderBottom: '1px solid var(--vesc-line)',
        fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
        color: 'var(--vesc-text-dim)', letterSpacing: '0.18em',
        textTransform: 'uppercase',
        background: 'var(--vesc-bg-1)',
      }}>
        <div style={{ display: 'flex', gap: 14 }}>
          <span style={{ color: 'var(--vesc-accent)', fontWeight: 700 }}>VESC · ONLINE</span>
          <span>MODE {d.mode} · {d.modeName}</span>
          <span>T+{d.rideTime}</span>
        </div>
        <div style={{ display: 'flex', gap: 14 }}>
          <span>BT</span><span>GPS · 4SAT</span><span>14:32</span>
        </div>
      </div>

      {/* Hero row: speed + power side by side */}
      <div style={{
        position: 'absolute', top: 28, left: 0, right: 0, height: 200,
        display: 'grid', gridTemplateColumns: '1fr 1fr',
        borderBottom: '1px solid var(--vesc-line)',
      }}>
        <div style={{
          padding: '14px 24px',
          borderRight: '1px solid var(--vesc-line)',
          display: 'flex', flexDirection: 'column',
        }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
            color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
          }}>SPEED · KM/H</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 160, lineHeight: 0.85, color: 'var(--vesc-text)',
            letterSpacing: '-0.05em', marginTop: 4,
          }} className="num">
            {pad(d.speed)}
          </div>
          <div style={{ marginTop: 6 }}>
            <HBar value={d.speed} max={d.speedMax} height={5} color="var(--vesc-accent)" segments={15} />
          </div>
        </div>

        <div style={{
          padding: '14px 24px',
          display: 'flex', flexDirection: 'column',
          alignItems: 'flex-end',
        }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
            color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
          }}>POWER · kW</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 160, lineHeight: 0.85, color: 'var(--vesc-accent)',
            letterSpacing: '-0.05em', marginTop: 4,
          }} className="num">
            {(d.power / 1000).toFixed(1)}
          </div>
          <div style={{ marginTop: 6, width: '100%' }}>
            <HBar value={d.power} max={d.powerMax} height={5} color="var(--vesc-accent)" segments={15} />
          </div>
        </div>
      </div>

      {/* Data grid 4×3 */}
      <div style={{
        position: 'absolute', top: 228, left: 0, right: 0, bottom: 0,
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gridTemplateRows: '1fr 1fr',
        borderTop: '1px solid var(--vesc-line)',
      }}>
        <Cell label="BATTERY" value={d.battery + '%'} unit={`${d.voltage.toFixed(1)}V`}
          color={batColor} bar={d.battery} barMax={100} barColor={batColor} />
        <Cell label="RANGE" value={d.range} unit="km" />
        <Cell label="MOTOR T°" value={d.motorTemp} unit="°C"
          alarm={d.motorTemp > 80}
          bar={d.motorTemp} barMax={100}
          barColor={d.motorTemp > 80 ? 'var(--vesc-danger)' : d.motorTemp > 60 ? 'var(--vesc-warn)' : 'var(--vesc-accent)'} />
        <Cell label="ESC T°" value={d.controllerTemp} unit="°C"
          bar={d.controllerTemp} barMax={100}
          barColor={d.controllerTemp > 80 ? 'var(--vesc-danger)' : d.controllerTemp > 60 ? 'var(--vesc-warn)' : 'var(--vesc-accent)'} />

        <Cell label="CURRENT" value={d.motorCurrent.toFixed(1)} unit="A"
          bar={d.motorCurrent} barMax={d.motorCurrentMax} />
        <Cell label="RPM" value={d.rpm} unit=""
          bar={d.rpm} barMax={d.rpmMax} />
        <Cell label="TRIP" value={d.trip.toFixed(1)} unit="km" />
        <Cell label="ODO" value={Math.floor(d.odometer).toLocaleString()} unit="km" />
      </div>
    </div>
  );
}

window.DashGrid = DashGrid;


// ===== dash-v4-hud.jsx =====
// V4 — HALF ARC
// Полукруглый верхний spidometr (от 9 до 3 часов), под ним — крупная
// цифра скорости + узкие row-блоки с ключевыми данными. Чистый, читается
// в один взгляд.

function DashHalfArc() {
  const d = useLiveData(4);
  const batColor = d.battery > 50 ? 'var(--vesc-accent)' : d.battery > 20 ? 'var(--vesc-warn)' : 'var(--vesc-danger)';

  // arc segments — top half, 180°
  const SEG = 60;
  const filled = pct(d.speed, 0, d.speedMax);
  const cx = 400, cy = 240, R = 200;

  return (
    <div className="dash">
      <StatusBar data={d} />

      {/* arc */}
      <div style={{ position: 'absolute', top: 32, left: 0, right: 0, height: 220, overflow: 'hidden' }}>
        {Array.from({ length: SEG }).map((_, i) => {
          const a = -180 + (i / (SEG - 1)) * 180; // -180..0
          const isFilled = i / SEG <= filled;
          const isMajor = i % 5 === 0;
          const len = isMajor ? 22 : 14;
          const rad = (a) * Math.PI / 180;
          // outer point
          const x1 = cx + Math.cos(rad) * R;
          const y1 = cy + Math.sin(rad) * R;
          return (
            <div key={i} style={{
              position: 'absolute', left: x1, top: y1,
              width: 3, height: len,
              background: isFilled ? 'var(--vesc-accent)' : 'var(--vesc-line-2)',
              transformOrigin: 'top center',
              transform: `translate(-50%, 0) rotate(${a + 90}deg)`,
              borderRadius: 1,
            }} />
          );
        })}

        {/* tick numbers */}
        {[0, 15, 30, 45, 60].map((v) => {
          const a = -180 + (v / d.speedMax) * 180;
          const rad = a * Math.PI / 180;
          const x = cx + Math.cos(rad) * (R - 40);
          const y = cy + Math.sin(rad) * (R - 40);
          return (
            <div key={v} style={{
              position: 'absolute', left: x, top: y,
              transform: 'translate(-50%, -50%)',
              fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
              color: 'var(--vesc-text-dim)',
            }} className="num">{v}</div>
          );
        })}
      </div>

      {/* big speed below arc */}
      <div style={{
        position: 'absolute', top: 130, left: 0, right: 0,
        textAlign: 'center',
      }}>
        <div style={{
          fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
          fontSize: 200, lineHeight: 0.85, color: 'var(--vesc-text)',
          letterSpacing: '-0.05em',
        }} className="num">
          {pad(d.speed)}
        </div>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 12,
          color: 'var(--vesc-accent)', letterSpacing: '0.4em',
          marginTop: -8,
        }}>KM/H</div>
      </div>

      {/* bottom — 3 row blocks */}
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 0, height: 130,
        background: 'var(--vesc-bg-1)',
        borderTop: '1px solid var(--vesc-line)',
        display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)',
      }}>
        {[
          { l: 'BATTERY', big: d.battery + '%', sub: `${d.voltage.toFixed(1)}V · ${d.range}km`, color: batColor, bar: d.battery, barMax: 100, barColor: batColor },
          { l: 'POWER',   big: (d.power/1000).toFixed(2) + ' kW', sub: `${d.motorCurrent.toFixed(1)}A · ${d.rpm} RPM`, color: 'var(--vesc-accent)', bar: d.power, barMax: d.powerMax, barColor: 'var(--vesc-accent)' },
          { l: 'THERMAL', big: d.motorTemp + '°C', sub: `ESC ${d.controllerTemp}°C · TRIP ${d.trip.toFixed(1)}km`, color: 'var(--vesc-text)', bar: d.motorTemp, barMax: 100, barColor: d.motorTemp > 80 ? 'var(--vesc-danger)' : d.motorTemp > 60 ? 'var(--vesc-warn)' : 'var(--vesc-accent)' },
        ].map((s, i) => (
          <div key={i} style={{
            padding: '18px 22px',
            borderRight: i < 2 ? '1px solid var(--vesc-line)' : 'none',
            display: 'flex', flexDirection: 'column', justifyContent: 'center', gap: 6,
          }}>
            <div style={{
              fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
              color: 'var(--vesc-text-dim)', letterSpacing: '0.24em',
            }}>{s.l}</div>
            <div style={{
              fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
              fontSize: 38, lineHeight: 1, color: s.color,
            }} className="num">{s.big}</div>
            <HBar value={s.bar} max={s.barMax} height={3} color={s.barColor} />
            <div style={{
              fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
              color: 'var(--vesc-text-dim)', letterSpacing: '0.1em',
            }}>{s.sub}</div>
          </div>
        ))}
      </div>
    </div>
  );
}

window.DashHalfArc = DashHalfArc;


// ===== dash-v5-skeumorph.jsx =====
// V5 — TELEMETRY TAPE
// Фокус на графиках в реальном времени. Слева — небольшая скорость + батарея,
// справа — три бегущих графика истории: power, current, motor temp.
// Это "режим телеметрии" для гиков, которые хотят видеть тренды.

function DashTelemetry() {
  const d = useLiveData(5);
  const [history, setHistory] = React.useState({ power: [], current: [], motorTemp: [] });

  React.useEffect(() => {
    const id = setInterval(() => {
      setHistory((h) => ({
        power: [...h.power, d.power].slice(-80),
        current: [...h.current, d.motorCurrent].slice(-80),
        motorTemp: [...h.motorTemp, d.motorTemp].slice(-80),
      }));
    }, 200);
    return () => clearInterval(id);
  }, [d.power, d.motorCurrent, d.motorTemp]);

  const batColor = d.battery > 50 ? 'var(--vesc-accent)' : d.battery > 20 ? 'var(--vesc-warn)' : 'var(--vesc-danger)';

  // Sparkline as polyline
  const Sparkline = ({ data, max, color, width = 380, height = 88 }) => {
    if (data.length < 2) return null;
    const points = data.map((v, i) => {
      const x = (i / (80 - 1)) * width;
      const y = height - pct(v, 0, max) * height;
      return `${x.toFixed(1)},${y.toFixed(1)}`;
    }).join(' ');
    const lastY = height - pct(data[data.length - 1], 0, max) * height;
    return (
      <svg width={width} height={height} style={{ display: 'block' }}>
        {/* grid */}
        {[0.25, 0.5, 0.75].map((p, i) => (
          <line key={i} x1={0} x2={width} y1={height * p} y2={height * p}
            stroke="var(--vesc-line)" strokeDasharray="2 4" />
        ))}
        {/* fill */}
        <polyline
          points={`0,${height} ${points} ${width},${height}`}
          fill={color} opacity="0.14" />
        {/* line */}
        <polyline points={points} fill="none" stroke={color} strokeWidth="2" />
        {/* leading dot */}
        <circle cx={width} cy={lastY} r="3" fill={color} />
      </svg>
    );
  };

  return (
    <div className="dash">
      <StatusBar data={d} />

      {/* LEFT — speed + battery */}
      <div style={{
        position: 'absolute', top: 32, left: 0, bottom: 0, width: 280,
        background: 'var(--vesc-bg-1)',
        borderRight: '1px solid var(--vesc-line)',
        padding: '20px 24px',
        display: 'flex', flexDirection: 'column', gap: 18,
      }}>
        <div>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
          }}>SPEED · KM/H</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 160, lineHeight: 0.82, color: 'var(--vesc-text)',
            letterSpacing: '-0.05em', marginTop: 2,
          }} className="num">{pad(d.speed)}</div>
          <div style={{ marginTop: 4 }}>
            <HBar value={d.speed} max={d.speedMax} height={4} color="var(--vesc-accent)" segments={15} />
          </div>
        </div>

        <div style={{ height: 1, background: 'var(--vesc-line)' }} />

        <div>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
          }}>BATTERY</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
            fontSize: 64, lineHeight: 0.9, color: batColor,
          }} className="num">
            {d.battery}<span style={{ fontSize: 22, color: 'var(--vesc-text-dim)', fontWeight: 500 }}>%</span>
          </div>
          <div style={{ marginTop: 6 }}>
            <HBar value={d.battery} max={100} height={5} color={batColor} segments={20} />
          </div>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
            color: 'var(--vesc-text-dim)', marginTop: 8,
            display: 'flex', justifyContent: 'space-between',
          }}>
            <span>{d.voltage.toFixed(1)} V</span>
            <span>{d.range} km</span>
          </div>
        </div>

        <div style={{ flex: 1 }} />

        <div style={{
          display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8,
          paddingTop: 12, borderTop: '1px solid var(--vesc-line)',
        }}>
          <div>
            <div style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: 9, color: 'var(--vesc-text-dim)', letterSpacing: '0.2em' }}>TRIP</div>
            <div style={{ fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700, fontSize: 22, color: 'var(--vesc-text)' }} className="num">{d.trip.toFixed(1)} <span style={{ fontSize: 11, color: 'var(--vesc-text-dim)' }}>km</span></div>
          </div>
          <div>
            <div style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: 9, color: 'var(--vesc-text-dim)', letterSpacing: '0.2em' }}>WH/KM</div>
            <div style={{ fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700, fontSize: 22, color: 'var(--vesc-text)' }} className="num">{d.whPerKm}</div>
          </div>
        </div>
      </div>

      {/* RIGHT — 3 charts stacked */}
      <div style={{
        position: 'absolute', top: 32, left: 280, right: 0, bottom: 0,
        padding: '14px 18px',
        display: 'flex', flexDirection: 'column', gap: 8,
      }}>
        {[
          { label: 'POWER', data: history.power, max: d.powerMax, current: (d.power/1000).toFixed(2), unit: 'kW', color: '#b6ff2e' },
          { label: 'CURRENT', data: history.current, max: d.motorCurrentMax, current: d.motorCurrent.toFixed(1), unit: 'A', color: '#22d3ee' },
          { label: 'MOTOR TEMP', data: history.motorTemp, max: 100, current: d.motorTemp, unit: '°C', color: d.motorTemp > 80 ? '#ff3b30' : d.motorTemp > 60 ? '#ffb02e' : '#b6ff2e' },
        ].map((c, i) => (
          <div key={i} style={{
            flex: 1,
            background: 'var(--vesc-bg-1)',
            border: '1px solid var(--vesc-line)',
            padding: '10px 14px',
            display: 'flex', flexDirection: 'column',
          }}>
            <div style={{
              display: 'flex', justifyContent: 'space-between', alignItems: 'baseline',
              marginBottom: 4,
            }}>
              <div style={{
                fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
                color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
              }}>{c.label}</div>
              <div style={{
                fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
                fontSize: 26, lineHeight: 1, color: c.color,
              }} className="num">
                {c.current}<span style={{ fontSize: 12, color: 'var(--vesc-text-dim)', marginLeft: 3, fontWeight: 500 }}>{c.unit}</span>
              </div>
            </div>
            <div style={{ flex: 1, position: 'relative' }}>
              <Sparkline data={c.data} max={c.max} color={c.color} width={490} height={80} />
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

window.DashTelemetry = DashTelemetry;


// ===== dash-v6-strip.jsx =====
// V6 — QUAD TILES
// 4 равноценные плитки 2×2. Без главной метрики — все данные равны по
// весу. Каждая плитка с большой цифрой, мини-баром и контекстом.
// Хорошо для пользователей, которые хотят "приборную доску" без иерархии.

function DashQuad() {
  const d = useLiveData(6);
  const batColor = d.battery > 50 ? 'var(--vesc-accent)' : d.battery > 20 ? 'var(--vesc-warn)' : 'var(--vesc-danger)';
  const motorColor = d.motorTemp > 80 ? 'var(--vesc-danger)' : d.motorTemp > 60 ? 'var(--vesc-warn)' : 'var(--vesc-accent)';

  const Tile = ({ label, big, unit, sub, color = 'var(--vesc-text)', bar, barMax, barColor, corner }) => (
    <div style={{
      position: 'relative',
      padding: '20px 26px',
      borderRight: '1px solid var(--vesc-line)',
      borderBottom: '1px solid var(--vesc-line)',
      display: 'flex', flexDirection: 'column', justifyContent: 'space-between',
      background: 'var(--vesc-bg-0)',
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' }}>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
          color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
          textTransform: 'uppercase',
        }}>{label}</div>
        {corner && <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
          color: 'var(--vesc-text-faint)', letterSpacing: '0.18em',
        }}>{corner}</div>}
      </div>
      <div>
        <div style={{
          fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
          fontSize: 100, lineHeight: 0.85, color, letterSpacing: '-0.04em',
        }} className="num">
          {big}
          {unit && <span style={{
            fontSize: 32, color: 'var(--vesc-text-dim)', fontWeight: 500, marginLeft: 6,
          }}>{unit}</span>}
        </div>
        {bar !== undefined && (
          <div style={{ marginTop: 10 }}>
            <HBar value={bar} max={barMax} height={5} color={barColor || color} segments={18} />
          </div>
        )}
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
          color: 'var(--vesc-text-dim)', marginTop: 8,
          letterSpacing: '0.16em',
        }}>{sub}</div>
      </div>
    </div>
  );

  return (
    <div className="dash">
      {/* Tiny status strip */}
      <div style={{
        position: 'absolute', top: 0, left: 0, right: 0, height: 24,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '0 14px',
        fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
        color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
        textTransform: 'uppercase',
        background: 'var(--vesc-bg-1)',
        borderBottom: '1px solid var(--vesc-line)',
      }}>
        <div style={{ display: 'flex', gap: 14 }}>
          <span style={{ color: 'var(--vesc-accent)', fontWeight: 700 }}>● VESC</span>
          <span>{d.modeName}</span>
          <span>BT</span><span>GPS</span>
        </div>
        <div style={{ display: 'flex', gap: 14 }}>
          <span>TRIP {d.trip.toFixed(1)}km</span>
          <span>{d.rideTime}</span>
        </div>
      </div>

      {/* 2×2 grid */}
      <div style={{
        position: 'absolute', top: 24, left: 0, right: 0, bottom: 0,
        display: 'grid',
        gridTemplateColumns: '1fr 1fr',
        gridTemplateRows: '1fr 1fr',
        borderTop: '1px solid var(--vesc-line)',
      }}>
        <Tile
          label="Speed"
          big={pad(d.speed)} unit="km/h"
          color="var(--vesc-text)"
          bar={d.speed} barMax={d.speedMax} barColor="var(--vesc-accent)"
          sub={`AVG ${d.speedAvg} · TOP ${d.speedTopRide} · ${d.rpm} RPM`}
          corner="01"
        />
        <Tile
          label="Power"
          big={(d.power / 1000).toFixed(2)} unit="kW"
          color="var(--vesc-accent)"
          bar={d.power} barMax={d.powerMax} barColor="var(--vesc-accent)"
          sub={`${d.motorCurrent.toFixed(1)} A · ${d.whPerKm} Wh/km`}
          corner="02"
        />
        <Tile
          label="Battery"
          big={d.battery} unit="%"
          color={batColor}
          bar={d.battery} barMax={100} barColor={batColor}
          sub={`${d.voltage.toFixed(1)} V · ${d.range} km RANGE`}
          corner="03"
        />
        <Tile
          label="Thermal"
          big={d.motorTemp} unit="°C"
          color={motorColor}
          bar={d.motorTemp} barMax={100} barColor={motorColor}
          sub={`ESC ${d.controllerTemp}°C · MOTOR LIMIT 100°C`}
          corner="04"
        />
      </div>
    </div>
  );
}

window.DashQuad = DashQuad;


// ===== dash-bluetooth.jsx =====
// dash-bluetooth.jsx — экран подключения к телефону
// Тач-интерфейс. Список доступных устройств, индикатор сигнала, активная
// сессия. Простые виджеты: list rows + status block.

function DashBluetooth() {
  const [scanning, setScanning] = React.useState(false);
  const [connected, setConnected] = React.useState('iPhone · Pavel');

  const devices = [
    { name: 'iPhone · Pavel', kind: 'phone', rssi: -42, paired: true, last: '2 min ago' },
    { name: 'Garmin Edge 1040', kind: 'watch', rssi: -68, paired: true, last: 'yesterday' },
    { name: 'VESC App · Mac', kind: 'mac', rssi: -71, paired: true, last: '3 days' },
    { name: 'Unknown device', kind: 'phone', rssi: -82, paired: false, last: '—' },
  ];

  const SignalBars = ({ rssi }) => {
    const level = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
    return (
      <div style={{ display: 'flex', alignItems: 'flex-end', gap: 2, height: 14 }}>
        {[1, 2, 3, 4].map((i) => (
          <div key={i} style={{
            width: 4, height: 4 + i * 2,
            background: i <= level ? 'var(--vesc-accent)' : 'var(--vesc-line-2)',
            borderRadius: 1,
          }} />
        ))}
      </div>
    );
  };

  return (
    <div className="dash">
      {/* Top bar */}
      <div style={{
        position: 'absolute', top: 0, left: 0, right: 0, height: 56,
        background: 'var(--vesc-bg-1)',
        borderBottom: '1px solid var(--vesc-line)',
        display: 'flex', alignItems: 'center', padding: '0 24px',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
          <div style={{
            width: 40, height: 40, borderRadius: 8,
            background: 'var(--vesc-bg-2)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
            color: 'var(--vesc-text-dim)', fontSize: 24,
          }}>‹</div>
          <div>
            <div style={{
              fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
              fontSize: 24, color: 'var(--vesc-text)', letterSpacing: '0.04em',
              textTransform: 'uppercase', lineHeight: 1,
            }}>Bluetooth</div>
            <div style={{
              fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
              color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
              marginTop: 2,
            }}>WIRELESS · BLE 5.0</div>
          </div>
        </div>

        <button
          onClick={() => setScanning(!scanning)}
          style={{
            background: scanning ? 'var(--vesc-accent)' : 'var(--vesc-bg-2)',
            color: scanning ? 'var(--vesc-bg-0)' : 'var(--vesc-text)',
            border: '1px solid ' + (scanning ? 'var(--vesc-accent)' : 'var(--vesc-line-2)'),
            fontFamily: "'JetBrains Mono', monospace", fontSize: 12,
            letterSpacing: '0.2em', padding: '10px 22px', borderRadius: 6,
            cursor: 'pointer', textTransform: 'uppercase', fontWeight: 600,
          }}>
          {scanning ? 'Stop scan' : 'Scan'}
        </button>
      </div>

      {/* Connected card */}
      <div style={{
        position: 'absolute', top: 72, left: 24, right: 24, height: 96,
        background: 'var(--vesc-bg-1)',
        border: '1px solid var(--vesc-accent)',
        borderRadius: 8,
        padding: '14px 20px',
        display: 'flex', alignItems: 'center', gap: 18,
      }}>
        <div style={{
          width: 64, height: 64, borderRadius: 8,
          background: 'var(--vesc-accent-soft)',
          border: '1px solid var(--vesc-accent)',
          display: 'flex', alignItems: 'center', justifyContent: 'center',
          fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
          fontSize: 30, color: 'var(--vesc-accent)',
        }}>BT</div>
        <div style={{ flex: 1 }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            color: 'var(--vesc-accent)', letterSpacing: '0.2em',
            textTransform: 'uppercase',
          }}>● Connected</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
            fontSize: 28, color: 'var(--vesc-text)', marginTop: 2,
          }}>{connected}</div>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
            color: 'var(--vesc-text-dim)', marginTop: 2,
            letterSpacing: '0.1em',
          }}>VESC App v6.02 · Telemetry streaming · 250ms</div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
          <SignalBars rssi={-42} />
          <button style={{
            background: 'transparent', color: 'var(--vesc-danger)',
            border: '1px solid var(--vesc-danger)',
            fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
            letterSpacing: '0.2em', padding: '8px 16px', borderRadius: 6,
            cursor: 'pointer', textTransform: 'uppercase',
          }}>Disconnect</button>
        </div>
      </div>

      {/* Devices section header */}
      <div style={{
        position: 'absolute', top: 184, left: 24, right: 24,
        display: 'flex', justifyContent: 'space-between', alignItems: 'baseline',
      }}>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
          color: 'var(--vesc-text-dim)', letterSpacing: '0.3em',
          textTransform: 'uppercase',
        }}>Paired devices · {devices.filter(d => d.paired).length}</div>
        {scanning && (
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
            color: 'var(--vesc-accent)', letterSpacing: '0.2em',
          }}>● SCANNING...</div>
        )}
      </div>

      {/* Devices list */}
      <div style={{
        position: 'absolute', top: 212, left: 24, right: 24, bottom: 24,
        display: 'flex', flexDirection: 'column', gap: 8,
        overflow: 'hidden',
      }}>
        {devices.map((dev, i) => {
          const isConnected = dev.name === connected;
          return (
            <div key={i} onClick={() => !isConnected && setConnected(dev.name)} style={{
              height: 56, background: 'var(--vesc-bg-1)',
              border: '1px solid ' + (isConnected ? 'var(--vesc-accent)' : 'var(--vesc-line)'),
              borderRadius: 6, padding: '0 18px',
              display: 'flex', alignItems: 'center', gap: 16,
              cursor: 'pointer',
            }}>
              <div style={{
                width: 36, height: 36, borderRadius: 6,
                background: 'var(--vesc-bg-2)',
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
                color: 'var(--vesc-text-dim)', fontWeight: 700,
                letterSpacing: '0.1em',
              }}>
                {dev.kind === 'phone' ? '📱' : dev.kind === 'watch' ? '⌚' : '💻'}
              </div>
              <div style={{ flex: 1 }}>
                <div style={{
                  fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 600,
                  fontSize: 20, color: 'var(--vesc-text)', lineHeight: 1.1,
                }}>{dev.name}</div>
                <div style={{
                  fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
                  color: 'var(--vesc-text-dim)', letterSpacing: '0.14em',
                  marginTop: 2,
                }}>
                  {dev.paired ? `LAST · ${dev.last.toUpperCase()}` : 'NOT PAIRED'}
                  {' · '}{dev.rssi} dBm
                </div>
              </div>
              <SignalBars rssi={dev.rssi} />
              <div style={{
                fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
                color: isConnected ? 'var(--vesc-accent)' : 'var(--vesc-text-dim)',
                letterSpacing: '0.2em', minWidth: 100, textAlign: 'right',
              }}>
                {isConnected ? '● ACTIVE' : (dev.paired ? 'CONNECT ›' : 'PAIR ›')}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

window.DashBluetooth = DashBluetooth;


// ===== dash-mode.jsx =====
// dash-mode.jsx — Mode 1/2/3 (Eco / Normal / Sport)
// Тач-выбор профиля езды. Большие плитки, активный профиль выделен
// акцентом, видны лимиты для каждого режима.

function DashMode() {
  const [mode, setMode] = React.useState(2);

  const modes = [
    {
      id: 1, name: 'ECO', subtitle: 'Range mode',
      desc: 'Maximum range, smooth acceleration',
      power: '40%', speed: 25, current: 30, regen: 'High',
    },
    {
      id: 2, name: 'NORMAL', subtitle: 'Daily ride',
      desc: 'Balanced response, comfortable acceleration',
      power: '70%', speed: 45, current: 55, regen: 'Med',
    },
    {
      id: 3, name: 'SPORT', subtitle: 'Full power',
      desc: 'Maximum performance, aggressive throttle',
      power: '100%', speed: 60, current: 80, regen: 'Low',
    },
  ];

  return (
    <div className="dash">
      {/* Top bar */}
      <div style={{
        position: 'absolute', top: 0, left: 0, right: 0, height: 56,
        background: 'var(--vesc-bg-1)',
        borderBottom: '1px solid var(--vesc-line)',
        display: 'flex', alignItems: 'center', padding: '0 24px',
        justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
          <div style={{
            width: 40, height: 40, borderRadius: 8,
            background: 'var(--vesc-bg-2)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
            color: 'var(--vesc-text-dim)', fontSize: 24,
          }}>‹</div>
          <div>
            <div style={{
              fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
              fontSize: 24, color: 'var(--vesc-text)', letterSpacing: '0.04em',
              textTransform: 'uppercase', lineHeight: 1,
            }}>Ride Mode</div>
            <div style={{
              fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
              color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
              marginTop: 2,
            }}>PROFILE · TAP TO SELECT</div>
          </div>
        </div>
        <div style={{
          fontFamily: "'JetBrains Mono', monospace", fontSize: 11,
          color: 'var(--vesc-accent)', letterSpacing: '0.2em',
        }}>● ACTIVE: {modes[mode - 1].name}</div>
      </div>

      {/* Three big tiles */}
      <div style={{
        position: 'absolute', top: 76, left: 24, right: 24, bottom: 90,
        display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 12,
      }}>
        {modes.map((m) => {
          const active = m.id === mode;
          return (
            <div key={m.id} onClick={() => setMode(m.id)} style={{
              background: active ? 'var(--vesc-accent-soft)' : 'var(--vesc-bg-1)',
              border: '1px solid ' + (active ? 'var(--vesc-accent)' : 'var(--vesc-line)'),
              borderRadius: 8,
              padding: '20px 22px',
              display: 'flex', flexDirection: 'column',
              cursor: 'pointer', position: 'relative',
            }}>
              {/* corner number */}
              <div style={{
                fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
                fontSize: 20, color: active ? 'var(--vesc-accent)' : 'var(--vesc-text-faint)',
                letterSpacing: '0.1em',
              }}>0{m.id}</div>

              <div style={{
                fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 800,
                fontSize: 56, lineHeight: 0.9, marginTop: 4,
                color: active ? 'var(--vesc-accent)' : 'var(--vesc-text)',
                letterSpacing: '-0.02em',
              }}>{m.name}</div>

              <div style={{
                fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
                color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
                textTransform: 'uppercase', marginTop: 6,
              }}>{m.subtitle}</div>

              <div style={{
                fontFamily: "'Space Grotesk', sans-serif", fontSize: 12,
                color: 'var(--vesc-text-dim)', marginTop: 10,
                lineHeight: 1.4,
              }}>{m.desc}</div>

              <div style={{ flex: 1 }} />

              {/* limits */}
              <div style={{
                display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px 8px',
                paddingTop: 12, borderTop: '1px solid ' + (active ? 'var(--vesc-accent)' : 'var(--vesc-line)'),
                opacity: 0.95,
              }}>
                {[
                  { l: 'POWER', v: m.power },
                  { l: 'SPEED', v: m.speed + ' km/h' },
                  { l: 'CURRENT', v: m.current + ' A' },
                  { l: 'REGEN', v: m.regen },
                ].map((s, i) => (
                  <div key={i}>
                    <div style={{
                      fontFamily: "'JetBrains Mono', monospace", fontSize: 9,
                      color: 'var(--vesc-text-dim)', letterSpacing: '0.18em',
                    }}>{s.l}</div>
                    <div style={{
                      fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
                      fontSize: 18, color: 'var(--vesc-text)', marginTop: 1,
                    }} className="num">{s.v}</div>
                  </div>
                ))}
              </div>

              {active && (
                <div style={{
                  position: 'absolute', top: 14, right: 14,
                  fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
                  color: 'var(--vesc-accent)', letterSpacing: '0.2em',
                }}>● ACTIVE</div>
              )}
            </div>
          );
        })}
      </div>

      {/* Bottom action bar */}
      <div style={{
        position: 'absolute', left: 0, right: 0, bottom: 0, height: 76,
        background: 'var(--vesc-bg-1)',
        borderTop: '1px solid var(--vesc-line)',
        display: 'flex', alignItems: 'center', padding: '0 24px',
        gap: 12,
      }}>
        <div style={{ flex: 1 }}>
          <div style={{
            fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
            color: 'var(--vesc-text-dim)', letterSpacing: '0.2em',
          }}>CURRENT PROFILE</div>
          <div style={{
            fontFamily: "'Barlow Condensed', sans-serif", fontWeight: 700,
            fontSize: 22, color: 'var(--vesc-text)', marginTop: 2,
          }}>{modes[mode - 1].name} · {modes[mode - 1].subtitle}</div>
        </div>
        <button style={{
          background: 'var(--vesc-bg-2)', color: 'var(--vesc-text)',
          border: '1px solid var(--vesc-line-2)',
          fontFamily: "'JetBrains Mono', monospace", fontSize: 12,
          letterSpacing: '0.2em', padding: '14px 22px', borderRadius: 6,
          cursor: 'pointer', textTransform: 'uppercase',
        }}>Customize</button>
        <button style={{
          background: 'var(--vesc-accent)', color: 'var(--vesc-bg-0)',
          border: '1px solid var(--vesc-accent)',
          fontFamily: "'JetBrains Mono', monospace", fontSize: 12,
          letterSpacing: '0.2em', padding: '14px 28px', borderRadius: 6,
          cursor: 'pointer', textTransform: 'uppercase', fontWeight: 700,
        }}>Apply</button>
      </div>
    </div>
  );
}

window.DashMode = DashMode;


// ===== app.jsx =====
// app.jsx — финальная сборка
// Все 6 вариантов главного экрана + Bluetooth + Mode 1/2/3 в design canvas.
// Tweaks-панель управляет акцентным цветом (классика VESC + альтернативы).

const TWEAK_DEFAULTS = /*EDITMODE-BEGIN*/{
  "accent": "#b6ff2e",
  "accentSoft": "rgba(182,255,46,0.14)"
}/*EDITMODE-END*/;

const ACCENT_PRESETS = [
  { name: 'Acid Green',  color: '#b6ff2e', soft: 'rgba(182,255,46,0.14)' },
  { name: 'Cyan',        color: '#22d3ee', soft: 'rgba(34,211,238,0.14)' },
  { name: 'Amber',       color: '#ffb02e', soft: 'rgba(255,176,46,0.14)' },
  { name: 'Red',         color: '#ff3b30', soft: 'rgba(255,59,48,0.14)' },
  { name: 'Violet',      color: '#a78bfa', soft: 'rgba(167,139,250,0.16)' },
  { name: 'White',       color: '#f0f4f6', soft: 'rgba(240,244,246,0.10)' },
];

function applyAccent(color, soft) {
  document.documentElement.style.setProperty('--vesc-accent', color);
  document.documentElement.style.setProperty('--vesc-accent-soft', soft);
}

function VESCApp() {
  const [tweaks, setTweak] = useTweaks(TWEAK_DEFAULTS);

  React.useEffect(() => {
    applyAccent(tweaks.accent, tweaks.accentSoft);
  }, [tweaks.accent, tweaks.accentSoft]);

  const setPreset = (p) => setTweak({ accent: p.color, accentSoft: p.soft });

  return (
    <>
      <DesignCanvas>
        <DCSection
          id="main"
          title="VESC Dashboard · Main Screen"
          subtitle="6 направлений главного экрана. 800×480, тёмная тема, под LVGL.">
          <DCArtboard id="v1" label="V1 · Cockpit" width={800} height={480}>
            <DashCockpit />
          </DCArtboard>
          <DCArtboard id="v2" label="V2 · Glyph" width={800} height={480}>
            <DashGlyph />
          </DCArtboard>
          <DCArtboard id="v3" label="V3 · Data Grid" width={800} height={480}>
            <DashGrid />
          </DCArtboard>
          <DCArtboard id="v4" label="V4 · Half Arc" width={800} height={480}>
            <DashHalfArc />
          </DCArtboard>
          <DCArtboard id="v5" label="V5 · Telemetry" width={800} height={480}>
            <DashTelemetry />
          </DCArtboard>
          <DCArtboard id="v6" label="V6 · Quad Tiles" width={800} height={480}>
            <DashQuad />
          </DCArtboard>
        </DCSection>

        <DCSection
          id="aux"
          title="Дополнительные экраны"
          subtitle="Bluetooth-подключение и выбор режима. Тач-интерфейс.">
          <DCArtboard id="bt" label="Bluetooth · Pairing" width={800} height={480}>
            <DashBluetooth />
          </DCArtboard>
          <DCArtboard id="mode" label="Mode · Eco / Normal / Sport" width={800} height={480}>
            <DashMode />
          </DCArtboard>
        </DCSection>

        <DCSection
          id="notes"
          title="Заметки и допущения"
          subtitle="">
          <DCPostIt width={420}>
            <strong>Что внутри</strong><br/>
            6 вариантов главного экрана 800×480 с одним и тем же набором
            данных (скорость, заряд, напряжение, ток, мощность, температуры,
            пробег, RPM, время поездки). Цифры живые — слегка дрожат, чтобы
            видеть как UI реагирует.
            <br/><br/>
            <strong>Под LVGL</strong><br/>
            Все формы — простые: lv_label, lv_bar (горизонтальный/
            вертикальный, в т.ч. сегментированный), lv_arc (для V3/V5),
            lv_obj со сплошной заливкой. Без blur, без сложных градиентов,
            без теней. Любую вёрстку можно собрать в SquareLine Studio или
            кодом.
            <br/><br/>
            <strong>Шрифты</strong><br/>
            В макете — Barlow Condensed (для крупных цифр) и JetBrains Mono
            (для лейблов). Под LVGL ставь monтированные .c-шрифты:
            например Barlow_Condensed_Bold размеров 24/40/64/120/200 +
            JetBrainsMono_Regular 12/14.
            <br/><br/>
            <strong>Тач</strong><br/>
            Bluetooth и Mode сделаны с тач-логикой (можно тыкать в плитки
            и устройства). Главный экран — read-only во время езды,
            свайп влево/вправо переключает между ним, BT и Mode.
          </DCPostIt>

          <DCPostIt width={380}>
            <strong>Куда смотреть дальше</strong><br/>
            • Выбери 1–2 направления, которые ближе всего по духу.<br/>
            • Я могу добавить экраны: статистика поездки (после стопа),
            графики тока/мощности в реальном времени, диагностика и FAULT
            CODES, калибровка IMU/датчиков, профиль водителя.<br/>
            • Могу нарисовать состояния: низкий заряд, перегрев, потеря
            сигнала, регенеративное торможение, превышение скорости.<br/>
            • Если есть конкретный логотип/брендинг бренда велосипеда —
            пришли, врежу.
            <br/><br/>
            <strong>Tweaks справа →</strong> можно крутить акцентный цвет
            и сразу видеть как все 6 экранов перекрашиваются.
          </DCPostIt>
        </DCSection>
      </DesignCanvas>

      <TweaksPanel title="Tweaks">
        <TweakSection label="Акцентный цвет">
          <div style={{
            display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)',
            gap: 8, marginBottom: 10,
          }}>
            {ACCENT_PRESETS.map((p) => {
              const active = p.color.toLowerCase() === (tweaks.accent || '').toLowerCase();
              return (
                <button key={p.name} onClick={() => setPreset(p)} style={{
                  padding: '10px 8px',
                  background: active ? '#1f2937' : '#f8fafc',
                  border: '1.5px solid ' + (active ? p.color : '#e2e8f0'),
                  borderRadius: 6,
                  cursor: 'pointer',
                  display: 'flex', flexDirection: 'column',
                  alignItems: 'center', gap: 6,
                  fontFamily: 'system-ui, sans-serif', fontSize: 11,
                  color: active ? '#f1f5f9' : '#334155',
                  transition: 'all .15s',
                }}>
                  <div style={{
                    width: 28, height: 28, borderRadius: '50%',
                    background: p.color,
                    boxShadow: active ? `0 0 0 3px ${p.color}33` : 'none',
                  }} />
                  <span>{p.name}</span>
                </button>
              );
            })}
          </div>
          <TweakColor
            label="Custom"
            value={tweaks.accent}
            onChange={(v) => {
              // also update soft to a translucent version
              const m = v.match(/^#([0-9a-f]{6})$/i);
              let soft = tweaks.accentSoft;
              if (m) {
                const r = parseInt(m[1].slice(0, 2), 16);
                const g = parseInt(m[1].slice(2, 4), 16);
                const b = parseInt(m[1].slice(4, 6), 16);
                soft = `rgba(${r},${g},${b},0.14)`;
              }
              setTweak({ accent: v, accentSoft: soft });
            }}
          />
        </TweakSection>

        <TweakSection label="Подсказка">
          <div style={{
            fontFamily: 'system-ui, sans-serif', fontSize: 12,
            color: '#475569', lineHeight: 1.5,
          }}>
            Цвет применяется ко всем 8 экранам сразу — батареи, графики,
            прогресс-бары, активные состояния. Кислотно-зелёный — классика
            VESC; cyan и amber дают более "приборный" вид; белый — самый
            нейтральный для солнца.
          </div>
        </TweakSection>
      </TweaksPanel>
    </>
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(<VESCApp />);


