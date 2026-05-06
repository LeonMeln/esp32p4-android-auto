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
