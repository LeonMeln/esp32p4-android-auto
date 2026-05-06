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
