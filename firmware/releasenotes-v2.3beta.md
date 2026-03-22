# BambuHelper v2.3 Beta Release Notes

## AMS filament indicator (NEW)

- **Active filament on screen** - colored dot + filament type (e.g. "PLA Matte") shown on the bottom bar during PRINTING and IDLE screens
- Works with all AMS units (up to 4 units, 16 trays) and external spool (vt_tray)
- Black filament visibility - gray outline around the color dot so it's visible on dark backgrounds
- Falls back to WiFi signal display when no AMS is present or no tray is active
- Data parsed from MQTT pushall using memmem() raw scan (same proven pattern as H2C dual nozzle)

## Smooth gauge animations (NEW)

- **Smooth arc transitions** - gauge arcs interpolate smoothly to new values instead of jumping instantly
- Text values update immediately (no delay), only the arc animates
- Exponential smoothing at 4Hz with ~1s settle time

## Gauge flicker elimination

- **Text cache system** - gauge center is only cleared and redrawn when the displayed text actually changes (e.g. "220" to "221"), not on every MQTT update
- **Transparent text rendering** - text inside gauges uses transparent background to avoid rectangular artifacts overlapping the arc
- **Bottom bar WiFi filter** - WiFi RSSI noise no longer triggers bottom bar redraws when AMS filament indicator is shown instead

## Buzzer improvements

- **Test buzzer button** - cycle through all sounds (Print Finished, Error, Connected) directly from the web UI
- **Section renamed** - "Multi-Printer" section renamed to "Hardware & Multi-Printer" for discoverability

## Display improvements

- **Larger gauge labels** - gauge labels ("Nozzle", "Part", etc.) upgraded from Font 1 (8px) to Font 2 (16px), positioned closer to the gauge arc
- **Smaller labels option** - checkbox in Display settings to revert to the original smaller labels
- **Animated progress bar default** - shimmer effect now enabled by default
- **Bottom bar font upgrade** - bottom status bar changed from Font 1 (8px) to Font 2 (16px) for better readability
- **Default background color** - changed from dark navy (0x0861) to black (0x0000)
- **Multi-printer no longer beta** - removed BETA tag from multi-printer support

## Pong clock overhaul

- **Smooth fonts** - replaced pixelated GLCD+setTextSize with Font 7 (7-segment, 48px) for time and Font 2 for date - much cleaner look
- **Date no longer overlaps bricks** - repositioned date and brick rows to avoid collision
- **Colon centered** - colon properly centered between hour and minute digits, blinks cleanly
- **No more flickering digits** - digits only redraw when their value changes or during bounce animation, date redraws once per day
- **Better paddle AI** - paddle predicts ball landing position instead of tracking ball X directly; stays put when ball goes up instead of drifting to center
- **More natural ball movement** - enforced minimum horizontal speed (1.2) after paddle bounce to prevent near-vertical trajectories

## Cloud MQTT stability fix (IMPORTANT)

- **Fixed cloud reconnect loop** - cloud printers (H2C, H2D, H2S, P2S) could enter a disconnect/reconnect cycle every 5-100 seconds with TLS error 49 (access_denied). Root cause: static MQTT client ID caused session collisions on the Bambu Cloud broker when the previous TLS session hadn't fully closed server-side
- **Random client ID for cloud** - each cloud connection now uses a unique random client ID (like pybambu/Home Assistant), preventing broker session conflicts
- **Clean TLS on cloud reconnect** - TLS and MQTT objects are fully destroyed and recreated before each cloud reconnect, eliminating stale session state
- **Initial-only pushall for cloud** - cloud sends one pushall request after connecting (for immediate full status), but no periodic pushall - cloud broker pushes updates automatically and repeated publishing could trigger access_denied
- **Instant first connection** - removed unnecessary 30s delay before the first cloud reconnect attempt, reducing initial connection time from ~23s to ~3s
- **Smart backoff** - connections lasting less than 30s no longer reset the backoff counter, preventing aggressive reconnect spam when the broker is rejecting connections
- **Cloud reconnect interval 30s** - increased from 10s to reduce broker rate limit risk
- **CA certificate bundle for cloud** - cloud TLS now uses proper certificate verification instead of setInsecure()
- **Disconnect diagnostics** - logs PubSubClient rc code, TLS error, connection duration, and message count on every disconnect for easier debugging

## Connecting screen improvements

- **Slide bar animation** - replaced the rotating arc spinner with a smooth sliding progress bar on both WiFi and MQTT connecting screens (fixes visual glitch where the arc would briefly show as a full circle during wrap-around)

## Display fixes

- **Pong clock text size bug** - switching from Pong clock to printer dashboard no longer shows garbled oversized text (tft.setTextSize was not reset)
- **ETA fallback fix** - ETA display no longer intermittently falls back to "Remaining: Xh XXm" after DST implementation (race condition in getLocalTime with timeout 0)
- **Finish buzzer repeating on rotation** - buzzer no longer replays the "print finished" sound every time the display rotates back to a finished printer (per-printer flag, reset on next print start)
- **Gauge text cache overflow** - increased cache from 8 to 12 slots to cover all screen layouts (printing: 6, idle: 2, finished: 2), preventing occasional gauge flicker on screen transitions
- **Gauge text cache null-terminator** - fixed missing null-terminator in force-redraw path that could cause stale text comparison reads past buffer
- **Smooth animation float comparison** - replaced exact float != with epsilon-based comparison to prevent edge-case animation jitter

## Code quality improvements

- **MQTT parser bounds checks** - added payload boundary validation to whitespace-skip loops after memmem() raw scans (extruder, ams, vt_tray parsing)
- **Safe string copies** - replaced strcpy/strncpy with strlcpy for gcodeState initialization and gauge text cache
- **Goto removal** - refactored goto wifi_fallback in bottom status bar to a helper function

## Build stats

- Flash: 89.3% (1170KB / 1310KB) - higher due to CA certificate bundle for cloud TLS
- RAM: 15.7% (51KB / 328KB)
- Board: lolin_s3_mini (ESP32-S3)
