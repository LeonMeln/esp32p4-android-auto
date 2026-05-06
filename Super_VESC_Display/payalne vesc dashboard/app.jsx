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
          <DCArtboard id="v1" label="Cockpit" width={800} height={480}>
            <DashCockpit />
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
