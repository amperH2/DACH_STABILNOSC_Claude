"""Read-only startup gate; no power, mount or roof commands."""
import math
import time
from weather_contract import policy, signature
from power_control import BUNDLE, LOGS, Blocked, Client, credentials, load_json

HOLD_SECONDS = 180
POLL_SECONDS = 5


def number(value):
    return type(value) in (int, float) and math.isfinite(value)


def observation(state):
    if not isinstance(state, dict):
        return None, 'Brak stanu ESP32'
    if not str(state.get('fw', '')).startswith('ASTRO-ROOF SAFE RC5.2 '):
        return None, 'Niepotwierdzona wersja RC5.2'
    if state.get('manual_mode') is not False:
        return None, 'Wymagany tryb AUTO'
    if state.get('safe') is not True:
        return None, 'ESP32 nie potwierdza SAFE'
    s, limits, diag = (state.get(k) for k in ('sensors', 'limits', 'diag'))
    if not all(isinstance(x, dict) for x in (s, limits, diag)):
        return None, 'Niepelne dane'
    try:
        p=policy(state)
    except Blocked as exc:
        return None,str(exc)
    if p is not None and not p['clouds']:
        uptime=diag.get('uptime_s')
        if not number(uptime) or uptime<0:return None,'Brak czasu pracy ESP32'
        return (0,signature(p),uptime),'Wybrane warunki SAFE; chmury poza decyzja'
    active = s.get('mlx_active')
    if type(active) is not int or active not in (1, 2):
        return None, 'Brak aktywnego MLX'
    age = s.get(f'mlx{active}_age_ms')
    if s.get(f'mlx{active}_valid') is not True or not number(age) or not 0 <= age <= 3000:
        return None, 'Brak swiezego, wiarygodnego pomiaru aktywnego MLX'
    delta, clear, uptime = s.get('cloud_delta'), limits.get('cloud_clear'), diag.get('uptime_s')
    if not all(number(x) for x in (delta, clear, uptime)) or not 5 <= clear <= 45 or uptime < 0:
        return None, 'Brak poprawnej delty, progu CLEAR lub czasu ESP32'
    if delta < clear:
        return None, f'Niebo jeszcze nie jest czyste: delta {delta:.1f}, CLEAR {clear:g}'
    return (active, (clear,signature(p)), uptime), 'Czyste niebo'


class ClearGate:
    def __init__(self):
        self.start = None
        self.previous = None

    def reset(self):
        self.start = self.previous = None

    def update(self, state, now):
        sample, reason = observation(state)
        if sample is None:
            self.reset()
            return False, 0, reason
        active, clear, uptime = sample
        if self.previous is not None:
            last_time, last_active, last_clear, last_uptime = self.previous
            gap, advance = now - last_time, uptime - last_uptime
            if (not 0 < gap <= 10 or not 0 < advance <= gap + 5
                    or active != last_active or clear != last_clear):
                self.reset()
        if self.start is None:
            self.start = now
        self.previous = (now, active, clear, uptime)
        elapsed = max(0, now - self.start)
        p=state.get('weather_policy')
        firmware_ready=p is None or p.get('open_ready') is True
        if not firmware_ready:reason+='; ESP32 czeka na stabilna poprawe'
        return elapsed >= HOLD_SECONDS and firmware_ready, min(elapsed, HOLD_SECONDS), reason


def wait_clear(api, clock=time.monotonic, sleep=time.sleep, report=print):
    gate = ClearGate()
    while True:
        try:
            state = api.esp()
        except Blocked as exc:
            if 'HTTP ' in str(exc) or 'Nieoczekiwany adres' in str(exc):
                raise
            gate.reset()
            report('Czekam: brak poprawnej odpowiedzi ESP32; licznik 0/180 s.')
        else:
            ready, elapsed, reason = gate.update(state, clock())
            report(f'{reason}; potwierdzenie warunkow {int(elapsed)}/180 s. Oczekiwanie przed krokiem Inveo.')
            if ready:
                return
        sleep(POLL_SECONDS)


def main():
    LOGS.mkdir(exist_ok=True)
    path = LOGS / 'START_NIEBO.txt'
    def report(message):
        print(message, flush=True)
        path.write_text(message + '\n', encoding='utf-8')
    try:
        cfg = load_json(BUNDLE / 'USTAWIENIA/config.json')
        wait_clear(Client(cfg, credentials()), report=report)
        report('Gotowe: wybrane warunki potwierdzone; NINA moze przejsc do Inveo.')
        return 0
    except (Blocked, OSError, KeyboardInterrupt) as exc:
        report('Start niepotwierdzony: ' + (str(exc) or 'Przerwano oczekiwanie'))
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
