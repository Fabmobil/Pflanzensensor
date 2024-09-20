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

const char htmlFooter[] PROGMEM = (R"====(
 &nbsp;
</div>
</div>
  <script type="text/javascript">
    onload = () => {
      const c = setTimeout(() => {
        document.body.classList.remove("not-loaded");
        clearTimeout(c);
      }, 1000);
    };
  </script>
  </body>
</html>
)====");

#endif // WIFI_FOOTER_H
