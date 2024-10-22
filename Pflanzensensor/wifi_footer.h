/**
 * @file wifi_footer.h
 * @brief HTML-Footer für Webseiten des Pflanzensensors
 * @author Tommy
 * @date 2023-09-20
 *
 * Diese Datei enthält den HTML-Footer, der auf allen Webseiten
 * des Pflanzensensors verwendet wird.
 */

#ifndef WIFI_FOOTER_H
#define WIFI_FOOTER_H

const char htmlFooter[] PROGMEM = R"=====(
 &nbsp;
</div>
</div>
  <script>
    // Warte bis Styles geladen sind
    if (document.readyState === 'complete') {
      setTimeout(() => document.body.classList.remove('not-loaded'), 100);
    } else {
      window.addEventListener('load', () => {
        setTimeout(() => document.body.classList.remove('not-loaded'), 100);
      });
    }
  </script>
  </body>
</html>
)=====";

#endif // WIFI_FOOTER_H
