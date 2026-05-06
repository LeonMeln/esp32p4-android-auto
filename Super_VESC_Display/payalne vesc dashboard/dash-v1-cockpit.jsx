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
