#!/usr/bin/env bash
#
# Baut die Firmware mit -fstack-usage und meldet Funktionen mit großem
# Stapelrahmen im EIGENEN Quelltext.
#
# Warum das eine eigene Prüfung verdient: der ESP8266 gibt dem Loop-Task nur
# 4 KB Stapel. Eine einzelne Funktion mit 1,6 KB Rahmen verbraucht davon ein
# Viertel, und weil die Aufrufkette tief ist (loop -> Webserver -> Handler ->
# Dateisystem), reicht das für einen Überlauf. Der äußert sich als
# "Panic core_esp8266_main.cpp:215 loop_task" - ein Absturz ohne Zeile und
# ohne Hinweis darauf, wer den Platz verbraucht hat.
#
# Genau das ist beim Sichern der Preferences passiert: die Funktion war über
# Jahre gewachsen, niemand hat den Rahmen gemessen, und der Fehler zeigte sich
# erst, als eine weitere Zeile das Fass zum Überlaufen brachte. Die
# Rahmengrößen sind deshalb hier festgehalten, statt bei Bedarf neu geraten zu
# werden.
#
# Der Grenzwert ist bewusst knapp über dem heutigen Höchstwert gesetzt: die
# Prüfung schlägt an, wenn eine Funktion wächst, nicht wenn sie schon groß ist.
# Wer eine Funktion bewusst größer macht, hebt den Wert hier - dann steht die
# Entscheidung wenigstens im Diff.
#
# Aufruf:  ./check_stack.sh [-q] [Grenzwert]
#   -q   nur die Zusammenfassung ausgeben
# Rückgabewert: 0 = alles unter dem Grenzwert, 1 = zu große Rahmen, 2 = Baufehler

set -uo pipefail

QUIET=0
[ "${1:-}" = "-q" ] && { QUIET=1; shift; }

# Heutiger Höchstwert: 1184 Byte (handleDownloadConfig). Etwas Luft darüber,
# damit kleine Umbauten nicht sofort anschlagen.
GRENZE="${1:-1280}"

cd "$(dirname "$0")" || exit 2

ENV_NAME=Pflanzensensor
BUILD_DIR=".pio/build/$ENV_NAME"

[ "$QUIET" = "1" ] || echo "Baue mit -fstack-usage..."
if ! PLATFORMIO_BUILD_FLAGS="-fstack-usage" pio run -e "$ENV_NAME" >/dev/null 2>&1; then
  echo "Baufehler - siehe 'pio run'" >&2
  exit 2
fi

# Die .su-Dateien liegen neben den Objektdateien. Nur die eigenen Quellen
# interessieren; Framework und Bibliotheken lassen sich ohnehin nicht ändern.
BEFUNDE=$(find "$BUILD_DIR" -name '*.su' -path '*src*' -print0 2>/dev/null |
  xargs -0 cat 2>/dev/null |
  awk -F'\t' -v grenze="$GRENZE" '
    {
      split($1, ort, ":")
      if ($2 + 0 >= 1)
        print $2, ort[1] ":" ort[2], $3
    }' | sort -rn)

if [ -z "$BEFUNDE" ]; then
  echo "Keine .su-Dateien gefunden - lief der Bau wirklich durch?" >&2
  exit 2
fi

UEBER=$(echo "$BEFUNDE" | awk -v grenze="$GRENZE" '$1 + 0 > grenze')

if [ "$QUIET" = "0" ]; then
  echo
  echo "Die zehn größten Stapelrahmen im eigenen Quelltext:"
  echo "$BEFUNDE" | head -10 | awk '{printf "  %6s B  %s\n", $1, $2}'
  echo
fi

ANZAHL=$(echo "$BEFUNDE" | wc -l)
if [ -n "$UEBER" ]; then
  echo "Über dem Grenzwert von $GRENZE Byte:"
  echo "$UEBER" | awk '{printf "  %6s B  %s\n", $1, $2}'
  echo "Geprüft: $ANZAHL Funktionen, Grenzwert $GRENZE Byte - ÜBERSCHRITTEN"
  exit 1
fi

echo "Geprüft: $ANZAHL Funktionen, größter Rahmen $(echo "$BEFUNDE" | head -1 | awk '{print $1}') B von $GRENZE B erlaubt"
exit 0
