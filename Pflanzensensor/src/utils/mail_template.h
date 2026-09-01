/**
 * @file mail_template.h
 * @brief Platzhalter in Mailvorlagen ersetzen - ohne Hardware
 * @details Header-only und frei von Arduino, LittleFS und Sensoren, wie
 *          utils/chronik_format.h und utils/mail_scheduler.h. So ist die
 *          Ersetzung nativ prüfbar; am Gerät ließe sie sich nur beobachten,
 *          indem man sich eine Mail schickt.
 *
 *          Dieselbe Ersetzung gibt es ein zweites Mal in JavaScript
 *          (data/js/mailvorlagen.js) für die Vorschau in der Weboberfläche.
 *          Damit beide Seiten sich einig bleiben, tragen
 *          test/test_mail_template/ und test/js/mailvorlagen.test.mjs
 *          dieselbe durchnummerierte Falltabelle.
 */

#ifndef MAIL_TEMPLATE_H
#define MAIL_TEMPLATE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace MailVorlage {

/// Längste Zeile, die verschickt wird. Passt zu ZEILE_MAX in mail_sender.cpp;
/// RFC 5321 4.5.3.1.6 erlaubt 1000 Byte, aber der gestopfte Puffer dort hängt
/// an dieser Zahl.
static constexpr size_t ZEILE_MAX = 256;
/// Betreff: mehr passt nicht sauber in eine nach RFC 2047 gefaltete Kopfzeile.
static constexpr size_t BETREFF_MAX = 120;
/// Gemeinsames CSS. Reicht für ein paar Dutzend Regeln; die Grenze hält den
/// POST klein und die Datei sicher unter einem 8-KB-Block.
constexpr size_t STIL_MAX = 1200;
/// Rumpf je Vorlage. Die Grenze kommt nicht vom Flash, sondern vom POST: der
/// Webserver hält den urlencodierten Rumpf dreifach im Heap, und jedes \r\n
/// wird dabei zu %0D%0A.
static constexpr size_t RUMPF_MAX = 1600;

/// Ein Platzhalter und sein Wert.
struct Paar {
  const char* name;
  const char* wert;
};

/// Nimmt eine fertige Zeile entgegen. Am Gerät stopft und sendet sie, im Test
/// sammelt sie.
using ZeilenSenke = void (*)(const char* text, size_t length, void* context);

/**
 * @brief Erzeugt die Zeilen eines Blockplatzhalters
 * @details Funktionszeiger statt std::function: kein Heap, und im nativen Test
 *          verhält es sich genau wie am Gerät.
 */
using BlockGeber = void (*)(const char* name, ZeilenSenke aus, void* senkeContext, void* context);

struct Umgebung {
  const Paar* werte{nullptr};
  size_t anzahl{0};
  BlockGeber bloecke{nullptr}; ///< nullptr: Blockplatzhalter bleiben wörtlich stehen
  void* blockContext{nullptr};
};

/// @brief Zeichen, das in einem Platzhalternamen vorkommen darf
inline bool istNamensZeichen(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }

/// @brief Ist das ein Folgebyte einer UTF-8-Sequenz?
inline bool istFolgeByte(char c) { return (static_cast<unsigned char>(c) & 0xC0) == 0x80; }

/**
 * @brief Wert eines Platzhalters, oder nullptr wenn unbekannt
 * @param length Länge des Namens (er ist nicht nullterminiert)
 */
inline const char* wertVon(const Umgebung& u, const char* name, size_t length) {
  for (size_t i = 0; i < u.anzahl; i++) {
    const char* kandidat = u.werte[i].name;
    if (kandidat && strlen(kandidat) == length && strncmp(kandidat, name, length) == 0) {
      return u.werte[i].wert ? u.werte[i].wert : "";
    }
  }
  return nullptr;
}

/// Namen der beiden Blockplatzhalter. Web, JavaScript und Kern müssen dieselben
/// kennen, deshalb stehen sie hier und nicht verstreut im Code.
static constexpr const char* BLOCK_MESSWERTE = "messwerte";
static constexpr const char* BLOCK_AUFFAELLIGE = "auffaellige";

inline bool istBlock(const char* name, size_t length) {
  return (length == strlen(BLOCK_MESSWERTE) && strncmp(name, BLOCK_MESSWERTE, length) == 0) ||
         (length == strlen(BLOCK_AUFFAELLIGE) && strncmp(name, BLOCK_AUFFAELLIGE, length) == 0);
}

/**
 * @brief Steht auf dieser Zeile nur ein Blockplatzhalter?
 * @param name Erhält Zeiger und Länge des Namens
 * @details Ein Block wirkt nur allein auf seiner Zeile. Stünde er mitten im
 *          Text, müsste der Text davor und dahinter gepuffert und mit der
 *          ersten und letzten erzeugten Zeile verklebt werden - genau der
 *          Fall, der Speicher kostet und den niemand braucht. Leerraum davor
 *          und dahinter ist erlaubt, damit Einrückung im HTML nicht stört.
 */
inline bool istBlockZeile(const char* zeile, const char** name, size_t* length) {
  if (!zeile) {
    return false;
  }
  const char* p = zeile;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p != '{') {
    return false;
  }
  p++;
  const char* start = p;
  while (istNamensZeichen(*p)) {
    p++;
  }
  const size_t len = static_cast<size_t>(p - start);
  if (len == 0 || *p != '}') {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\r') {
    p++;
  }
  if (*p != '\0') {
    return false; // es folgt noch Text - dann ist es kein Blockplatzhalter
  }
  if (!istBlock(start, len)) {
    return false;
  }
  if (name)
    *name = start;
  if (length)
    *length = len;
  return true;
}

/**
 * @brief Länge, gekürzt auf vollständige UTF-8-Zeichen
 * @details Wird beim Lesen gebraucht: füllt eine überlange Zeile den
 *          Lesepuffer, steht der Rest noch in der Datei - und der Schnitt darf
 *          nicht mitten in eine Mehrbytefolge fallen. Sonst stünde im Text ein
 *          Ersatzzeichen, und beim nächsten Lesestück noch eines.
 */
inline size_t ganzeZeichen(const char* text, size_t length) {
  if (!text || length == 0) {
    return 0;
  }
  // Anfang des letzten Zeichens suchen
  size_t start = length - 1;
  while (start > 0 && istFolgeByte(text[start])) {
    start--;
  }
  const unsigned char kopf = static_cast<unsigned char>(text[start]);
  size_t erwartet = 1;
  if ((kopf & 0xE0) == 0xC0)
    erwartet = 2;
  else if ((kopf & 0xF0) == 0xE0)
    erwartet = 3;
  else if ((kopf & 0xF8) == 0xF0)
    erwartet = 4;

  return (start + erwartet <= length) ? length : start;
}

/**
 * @brief Wo darf eine zu lange Zeile getrennt werden?
 * @return Anzahl Bytes, die in die erste Zeile dürfen
 * @details Bevorzugt am letzten Leerzeichen, sonst an der letzten
 *          UTF-8-Zeichengrenze. Mitten in einer Mehrbytefolge zu trennen
 *          ergäbe beim Empfänger ein Ersatzzeichen - und Emojis sind hier vier
 *          Byte lang.
 */
inline size_t trennstelle(const char* text, size_t length, size_t max) {
  if (length <= max) {
    return length;
  }
  // Rückwärts nach einem Leerzeichen suchen, aber nicht beliebig weit: sonst
  // entstünde aus einer langen Attributzeile eine winzige erste Zeile.
  const size_t mindestens = max / 2;
  for (size_t i = max; i > mindestens; i--) {
    if (text[i - 1] == ' ') {
      return i;
    }
  }
  size_t schnitt = max;
  while (schnitt > 0 && istFolgeByte(text[schnitt])) {
    schnitt--;
  }
  return schnitt > 0 ? schnitt : max;
}

/**
 * @brief Trennstelle nach vorn schieben, bis sie ausserhalb von Tags liegt
 * @details Der Text ist ab hier maschinell erzeugtes HTML. Ein Umbruch mitten
 *          in `<strong>` oder in `&amp;` waere fuer den Empfaenger sichtbarer
 *          Schrott - beim Tag, weil das Attribut zerfaellt, bei der Entitaet,
 *          weil sie ohne Semikolon nicht mehr aufgeloest wird. Innerhalb eines
 *          Tags waere ein Umbruch nach HTML zwar erlaubt, aber die Zeile geht
 *          durch SMTP, und dort ist CRLF eine Zeilengrenze.
 */
inline size_t ausserhalbMarkup(const char* text, size_t schnitt) {
  size_t offen = 0; // Position des zuletzt geoeffneten < oder &
  bool imTag = false;
  bool inEntitaet = false;
  for (size_t i = 0; i < schnitt; i++) {
    const char c = text[i];
    if (imTag) {
      if (c == '>')
        imTag = false;
      continue;
    }
    if (inEntitaet) {
      // Ein & ohne Semikolon ist keine Entitaet; nach ein paar Zeichen aufgeben,
      // sonst haelt ein einzelnes kaufmaennisches Und die halbe Zeile fest.
      if (c == ';' || i - offen > 8)
        inEntitaet = false;
      continue;
    }
    if (c == '<') {
      imTag = true;
      offen = i;
    } else if (c == '&') {
      inEntitaet = true;
      offen = i;
    }
  }
  if ((imTag || inEntitaet) && offen > 0) {
    return offen;
  }
  return schnitt;
}

namespace detail {

/**
 * @class Ausgeber
 * @brief Sammelt expandierten Text und gibt ihn zeilenweise aus
 * @details Sobald mehr als eine Zeile beisammen ist, wird sie ausgegeben und
 *          der Rest rückt nach. Damit darf eine Vorlagenzeile beliebig lang
 *          werden, ohne dass mehr als ein halbes Kilobyte Stack nötig wäre -
 *          und ohne dass etwas abgeschnitten wird. Abschneiden wäre hier die
 *          schlechteste Lösung: es zerreißt HTML mitten im Tag.
 */
class Ausgeber {
public:
  Ausgeber(ZeilenSenke aus, void* context) : m_aus(aus), m_context(context) {}

  void schiebe(const char* text, size_t length) {
    while (length > 0) {
      const size_t platz = sizeof(m_puffer) - m_at;
      const size_t nimm = length < platz ? length : platz;
      memcpy(m_puffer + m_at, text, nimm);
      m_at += nimm;
      text += nimm;
      length -= nimm;
      while (m_at > ZEILE_MAX) {
        gibZeileAus();
      }
    }
  }

  void schiebe(char c) { schiebe(&c, 1); }

  /// Rest ausgeben. Auch ein leerer Rest ergibt eine Zeile - eine Leerzeile in
  /// der Vorlage soll eine Leerzeile in der Mail bleiben.
  void fertig() {
    while (m_at > ZEILE_MAX) {
      gibZeileAus();
    }
    m_aus(m_puffer, m_at, m_context);
    m_at = 0;
  }

private:
  void gibZeileAus() {
    size_t schnitt = trennstelle(m_puffer, m_at, ZEILE_MAX);
    const size_t sicher = ausserhalbMarkup(m_puffer, schnitt);
    // Nur uebernehmen, wenn dabei etwas uebrig bleibt: eine Trennstelle bei 0
    // wuerde die Schleife in schiebe() nie beenden.
    if (sicher > 0) {
      schnitt = sicher;
    }
    m_aus(m_puffer, schnitt, m_context);
    memmove(m_puffer, m_puffer + schnitt, m_at - schnitt);
    m_at -= schnitt;
  }

  // Eine Zeile plus etwas Reserve. Nicht die doppelte Zeilenlänge: dieser
  // Puffer liegt auf dem Stack, und der ESP8266 hat davon nur 4 KB - die Kette
  // bis hierher ist tief.
  char m_puffer[ZEILE_MAX + 64];
  size_t m_at{0};
  ZeilenSenke m_aus;
  void* m_context;
};

} // namespace detail

namespace detail {

/// @brief Ein Zeichen ausgeben, HTML-Sonderzeichen als Entität
inline void schiebeMaskiert(Ausgeber& aus, char c) {
  switch (c) {
  case '&':
    aus.schiebe("&amp;", 5);
    break;
  case '<':
    aus.schiebe("&lt;", 4);
    break;
  case '>':
    aus.schiebe("&gt;", 4);
    break;
  case '"':
    aus.schiebe("&quot;", 6);
    break;
  default:
    aus.schiebe(c);
  }
}

/**
 * @brief Platzhalter ersetzen, sonst nichts
 * @details Für den Betreff: er landet als RFC-2047-Kopfzeile in der Mail und
 *          ist dort Klartext. Eine Maskierung würde der Empfänger wörtlich als
 *          "&amp;" lesen.
 */
inline void schiebeKlartext(Ausgeber& aus, const char* p, const Umgebung& u) {
  while (*p) {
    if (p[0] == '{' && p[1] == '{') {
      aus.schiebe('{');
      p += 2;
      continue;
    }
    if (*p == '{') {
      const char* start = p + 1;
      const char* zu = start;
      while (istNamensZeichen(*zu)) {
        zu++;
      }
      const size_t len = static_cast<size_t>(zu - start);
      const char* wert = (len > 0 && *zu == '}') ? wertVon(u, start, len) : nullptr;
      if (wert) {
        aus.schiebe(wert, strlen(wert));
        p = zu + 1;
        continue;
      }
    }
    aus.schiebe(*p);
    p++;
  }
}

/**
 * @brief Klartext mit Platzhaltern ausgeben
 * @details Der Vorlagentext ist kein HTML mehr, also wird alles maskiert - der
 *          eingesetzte Wert genauso wie das Getippte. Ein Gerätename mit einem
 *          spitzen Klammeraffen darf die Mail nicht zerlegen.
 *
 *          Unbekannte Platzhalter bleiben wörtlich stehen, samt Klammern. So
 *          sieht der Nutzer seinen Tippfehler in der Mail, statt sich über
 *          eine unerklärliche Lücke zu wundern.
 */
inline void schiebeText(Ausgeber& aus, const char* p, const char* ende, const Umgebung& u) {
  while (p < ende) {
    if (p[0] == '{' && p + 1 < ende && p[1] == '{') {
      aus.schiebe('{');
      p += 2;
      continue;
    }
    if (*p == '{') {
      const char* start = p + 1;
      const char* zu = start;
      while (zu < ende && istNamensZeichen(*zu)) {
        zu++;
      }
      const size_t len = static_cast<size_t>(zu - start);
      const char* wert = (len > 0 && zu < ende && *zu == '}') ? wertVon(u, start, len) : nullptr;
      if (wert) {
        for (const char* w = wert; *w; w++) {
          schiebeMaskiert(aus, *w);
        }
        p = zu + 1;
        continue;
      }
    }
    schiebeMaskiert(aus, *p);
    p++;
  }
}

/// Nur diese Anfänge werden zu einem Link. Alles andere - allen voran
/// `javascript:` - bleibt Text; ein Mailprogramm soll aus unserer Vorlage
/// nichts ausführen können.
inline bool istErlaubteAdresse(const char* p, const char* ende) {
  const size_t len = static_cast<size_t>(ende - p);
  return (len > 7 && strncmp(p, "http://", 7) == 0) ||
         (len > 8 && strncmp(p, "https://", 8) == 0) || (len > 7 && strncmp(p, "mailto:", 7) == 0);
}

/**
 * @brief Auszeichnung innerhalb einer Zeile: **fett** und [Text](Adresse)
 * @details Bewusst winzig gehalten. Ohne schließende Zeichen bleibt alles
 *          Text - wer einen Stern schreibt, soll einen Stern sehen.
 */
inline void schiebeAuszeichnung(Ausgeber& aus, const char* p, const Umgebung& u) {
  const char* ende = p + strlen(p);
  const char* text = p;

  while (*p) {
    if (p[0] == '*' && p[1] == '*') {
      const char* zu = strstr(p + 2, "**");
      if (zu) {
        schiebeText(aus, text, p, u);
        aus.schiebe("<strong>", 8);
        schiebeText(aus, p + 2, zu, u);
        aus.schiebe("</strong>", 9);
        p = zu + 2;
        text = p;
        continue;
      }
    }

    if (*p == '[') {
      const char* txtEnde = strchr(p + 1, ']');
      if (txtEnde && txtEnde[1] == '(') {
        const char* adrEnde = strchr(txtEnde + 2, ')');
        if (adrEnde && istErlaubteAdresse(txtEnde + 2, adrEnde)) {
          schiebeText(aus, text, p, u);
          aus.schiebe("<a href=\"", 9);
          schiebeText(aus, txtEnde + 2, adrEnde, u);
          aus.schiebe("\">", 2);
          schiebeText(aus, p + 1, txtEnde, u);
          aus.schiebe("</a>", 4);
          p = adrEnde + 1;
          text = p;
          continue;
        }
      }
    }

    p++;
  }
  schiebeText(aus, text, ende, u);
}

} // namespace detail

/// @brief Besteht die Zeile nur aus Leerraum?
inline bool istLeerzeile(const char* zeile) {
  for (const char* p = zeile; *p; p++) {
    if (*p != ' ' && *p != '\t') {
      return false;
    }
  }
  return true;
}

/**
 * @brief Eine Vorlagenzeile in HTML umsetzen und ausgeben
 * @details Kann mehrere Zeilen erzeugen: durch einen Blockplatzhalter oder
 *          weil die expandierte Zeile zu lang wurde.
 *
 *          Der Vorlagentext enthält kein HTML mehr, sondern Klartext mit einer
 *          winzigen Auszeichnung: `# ` macht eine Überschrift, `**` fettet,
 *          `[Text](Adresse)` verlinkt. Die Gestaltung kommt aus dem
 *          gemeinsamen Stil, nicht aus dem Text - sonst müsste jeder, der ein
 *          Wort ändern will, HTML lesen können.
 *
 *          Jede nichtleere Zeile wird ein Absatz. Leere Zeilen dienen im
 *          Editor der Übersicht und erzeugen nichts; Abstände macht der Stil.
 */
inline void expandiereZeile(const char* zeile, const Umgebung& u, ZeilenSenke aus, void* context) {
  if (!zeile || !aus) {
    return;
  }

  const char* blockName = nullptr;
  size_t blockLen = 0;
  if (istBlockZeile(zeile, &blockName, &blockLen)) {
    if (!u.bloecke) {
      // Ohne Blockgeber bleibt der Platzhalter wörtlich stehen - dann sieht man
      // in der Vorschau, dass dort etwas eingesetzt würde.
      detail::Ausgeber roh(aus, context);
      roh.schiebe("<p>", 3);
      detail::schiebeText(roh, zeile, zeile + strlen(zeile), u);
      roh.schiebe("</p>", 4);
      roh.fertig();
      return;
    }
    char name[24];
    const size_t nimm = blockLen < sizeof(name) - 1 ? blockLen : sizeof(name) - 1;
    memcpy(name, blockName, nimm);
    name[nimm] = '\0';
    u.bloecke(name, aus, context, u.blockContext);
    return;
  }

  // Leerzeilen sind Gliederung im Editor, kein Inhalt der Mail.
  if (istLeerzeile(zeile)) {
    return;
  }

  const char* p = zeile;
  const bool ueberschrift = (p[0] == '#' && p[1] == ' ');
  if (ueberschrift) {
    p += 2;
  }

  detail::Ausgeber ausgeber(aus, context);
  ausgeber.schiebe(ueberschrift ? "<h1>" : "<p>", ueberschrift ? 4 : 3);
  detail::schiebeAuszeichnung(ausgeber, p, u);
  ausgeber.schiebe(ueberschrift ? "</h1>" : "</p>", ueberschrift ? 5 : 4);
  ausgeber.fertig();
}

/**
 * @brief Betreff expandieren
 * @return Länge ohne Nullbyte
 * @details CR und LF werden verworfen und Blockplatzhalter sind abgeschaltet.
 *          Ein Zeilenumbruch im Betreff wäre eine Kopfzeileneinschleusung -
 *          damit ließe sich ein zusätzliches "Bcc:" unterschieben.
 */
inline size_t expandiereBetreff(const char* zeile, const Umgebung& u, char* out, size_t outSize) {
  if (!zeile || !out || outSize == 0) {
    return 0;
  }

  Umgebung ohneBloecke = u;
  ohneBloecke.bloecke = nullptr;

  struct Ziel {
    char* out;
    size_t outSize;
    size_t at;
  } ziel{out, outSize, 0};

  auto senke = [](const char* text, size_t length, void* context) {
    Ziel* z = static_cast<Ziel*>(context);
    for (size_t i = 0; i < length && z->at + 1 < z->outSize; i++) {
      if (text[i] == '\r' || text[i] == '\n') {
        continue;
      }
      z->out[z->at++] = text[i];
    }
  };

  {
    detail::Ausgeber ausgeber(senke, &ziel);
    detail::schiebeKlartext(ausgeber, zeile, ohneBloecke);
    ausgeber.fertig();
  }
  out[ziel.at] = '\0';
  return ziel.at;
}

// === Format der Vorlagendatei ===

/// Kopfzeile. Fehlt sie, gilt die Datei als fremd und die Standardvorlagen
/// greifen - ein späterer Formatwechsel braucht dann keinen Migrationscode.
// MV2: die Rümpfe sind seit v2.32.0 Klartext mit Mini-Auszeichnung, kein HTML
// mehr. Eine MV1-Datei wird dadurch als fremd erkannt und der Standard greift -
// das ist gewollt, ihr HTML stünde sonst maskiert in der Mail.
static constexpr const char* DATEI_KOPF = "#MV2";

enum class Abschnitt : uint8_t {
  Keiner,
  BootBetreff,
  BootRumpf,
  WarnBetreff,
  WarnRumpf,
  AliveBetreff,
  AliveRumpf,
  Stil,     ///< gemeinsames CSS für alle drei Mailarten
  Unbekannt ///< sieht aus wie eine Marke, ist aber keine bekannte
};

inline const char* markeVon(Abschnitt a) {
  switch (a) {
  case Abschnitt::BootBetreff:
    return "[boot.betreff]";
  case Abschnitt::BootRumpf:
    return "[boot.rumpf]";
  case Abschnitt::WarnBetreff:
    return "[warnung.betreff]";
  case Abschnitt::WarnRumpf:
    return "[warnung.rumpf]";
  case Abschnitt::AliveBetreff:
    return "[alive.betreff]";
  case Abschnitt::AliveRumpf:
    return "[alive.rumpf]";
  case Abschnitt::Stil:
    return "[stil]";
  default:
    return "";
  }
}

/**
 * @brief Ist diese Zeile eine Abschnittsmarke?
 * @details Streng: beginnt mit '[', endet mit ']', dazwischen nur Kleinbuchstaben
 *          und Punkte, kein Leerraum. Die Datei schreibt nur das Gerät selbst -
 *          Nachsicht wäre hier bloß zusätzliche Angriffsfläche.
 */
inline Abschnitt erkenneMarke(const char* zeile) {
  if (!zeile || zeile[0] != '[') {
    return Abschnitt::Keiner;
  }
  const size_t length = strlen(zeile);
  if (length < 3 || zeile[length - 1] != ']') {
    return Abschnitt::Keiner;
  }
  for (size_t i = 1; i + 1 < length; i++) {
    const char c = zeile[i];
    if (!((c >= 'a' && c <= 'z') || c == '.')) {
      return Abschnitt::Keiner;
    }
  }

  const Abschnitt alle[] = {Abschnitt::BootBetreff, Abschnitt::BootRumpf,    Abschnitt::WarnBetreff,
                            Abschnitt::WarnRumpf,   Abschnitt::AliveBetreff, Abschnitt::AliveRumpf,
                            Abschnitt::Stil};
  for (Abschnitt a : alle) {
    if (strcmp(zeile, markeVon(a)) == 0) {
      return a;
    }
  }
  return Abschnitt::Unbekannt;
}

/**
 * @brief Muss diese Zeile beim Schreiben entwertet werden?
 * @details Eine HTML-Zeile wie "[1]" sähe sonst aus wie eine Abschnittsmarke
 *          und zerlegte die Datei. Dieselbe Denkweise wie das Punkt-Stuffing
 *          in SMTP (RFC 5321 4.5.2, siehe Smtp::stuffLine).
 */
inline bool brauchtEntwertung(const char* zeile) {
  return zeile && (zeile[0] == '[' || zeile[0] == '\\');
}

/// @brief Führendes Entwertungszeichen entfernen
inline const char* entwerte(const char* zeile) {
  if (zeile && zeile[0] == '\\') {
    return zeile + 1;
  }
  return zeile;
}

/// Alle bekannten Platzhalternamen - für die Prüfung in der Weboberfläche und
/// als einzige Quelle der Wahrheit gegenüber dem JavaScript.
inline bool istBekannt(const char* name, size_t length) {
  static const char* NAMEN[] = {"geraet",   "name",  "ip",      "ssid",          "neustarts",
                                "laufzeit", "datum", "uhrzeit", BLOCK_MESSWERTE, BLOCK_AUFFAELLIGE};
  for (const char* n : NAMEN) {
    if (strlen(n) == length && strncmp(n, name, length) == 0) {
      return true;
    }
  }
  return false;
}

} // namespace MailVorlage

#endif // MAIL_TEMPLATE_H
