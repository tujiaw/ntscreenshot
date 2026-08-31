---
name: query-weather
description: Query current weather or short-term forecast with wttr.in. Use when the user asks about weather, temperature, rain, snow, wind, humidity, forecast, clothing advice based on weather, or relative dates such as today, tomorrow, this weekend, and next week.
---

# Query Weather

Use `wttr.in` as the weather source. Prefer machine-readable output and deterministic parsing.

Official reference: https://github.com/chubin/wttr.in

## Core Rules

- Always query live weather from `wttr.in`; do not answer from memory.
- Prefer JSON output with `?format=j1` for normal weather questions.
- Use `?format=j2` only when a smaller JSON payload is enough.
- Avoid parsing ANSI or decorative terminal output unless the user explicitly wants the raw `wttr.in` text style.
- Always include the resolved location and exact date in the final answer.
- If the user uses relative dates such as `today` or `tomorrow`, convert them to exact dates in the reply.
- If the location is ambiguous, ask one short clarifying question instead of guessing.

## Query Method

Build the query from the user's intent:

1. Resolve location.
   - City or district: `/Location`
   - Multi-word place: replace spaces with `+`, such as `/New+York`
   - Special place name: prefix with `~`, such as `/~Eiffel+Tower`
   - Domain-based lookup: prefix with `@`, such as `/@github.com`
   - If the user gives no location, `wttr.in` may use IP-based location, but prefer asking for the city when correctness matters.

2. Pick units explicitly.
   - Use `?m` for metric SI
   - Use `?M` for metric with wind speed in `m/s`
   - Use `?u` for USCS
   - Do not rely on wttr.in automatic region-based defaults when the user asked for a specific unit style.

3. Pick format.
   - Default: `?format=j1`
   - Lightweight JSON: `?format=j2`
   - Human-readable one-line output only when useful: `?format=3` or `?format=4`
   - Raw plain text can be forced with `?T`, but do not use it as the primary parsing format.

4. Pick language if needed.
   - Use `&lang=zh` for Chinese when replying in Chinese and the returned text fields need localization.
   - If using JSON and translating the summary yourself, `lang` is optional.

## What To Read From JSON

When using `format=j1`, prioritize these fields:

- `current_condition[0].temp_C` / `temp_F`
- `current_condition[0].FeelsLikeC` / `FeelsLikeF`
- `current_condition[0].humidity`
- `current_condition[0].precipMM`
- `current_condition[0].windspeedKmph` / `windspeedMiles`
- `current_condition[0].winddir16Point`
- `current_condition[0].weatherDesc[0].value`
- `current_condition[0].uvIndex`
- `weather[]` for daily forecast
- `nearest_area[]` for resolved place name if needed

## Edge Cases

- For `today` or `now`, use `current_condition`.
- For `tomorrow` or short forecast requests, use the corresponding item under `weather[]`.
- For weekend or multi-day planning, summarize trend, min/max temperature, precipitation, and wind instead of dumping every hour.
- If the user asks whether they need an umbrella or coat, give a short practical recommendation based on temperature, precipitation, and wind.
- If `wttr.in` returns an unexpected location, say that explicitly and ask whether to retry with a more precise place name.
- If the service is unavailable or returns malformed data, say you could not verify the weather from `wttr.in` right now instead of guessing.

## Response Shape

Use a compact answer:

`[Location] [Exact Date]: [Condition], [temperature], [feels like], [precipitation], [wind].`

Add one short sentence only if useful:
- umbrella or clothing advice
- timing of rain or snow
- notable uncertainty or service limitation

## Example Query Patterns

- Current weather in Beijing:
  `https://wttr.in/Beijing?format=j1&m`
- Current weather in Shanghai with wind in m/s:
  `https://wttr.in/Shanghai?format=j1&M`
- Chinese text preference:
  `https://wttr.in/Hangzhou?format=j1&m&lang=zh`
- Lightweight JSON:
  `https://wttr.in/Guangzhou?format=j2&m`
