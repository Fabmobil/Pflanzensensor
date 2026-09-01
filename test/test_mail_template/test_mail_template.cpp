/**
 * @file test_mail_template.cpp
 * @brief Tests für die Platzhalterersetzung (utils/mail_template.h)
 *
 * FALLTABELLE - dieselben Nummern stehen in test/js/mailvorlagen.test.mjs, weil
 * die Ersetzung ein zweites Mal in JavaScript existiert (Vorschau in der
 * Weboberfläche). Läuft eine Seite aus dem Tritt, fällt es hier oder dort auf,
 * statt beim Empfänger der Mail:
 *
 *   1  Zeile ohne Platzhalter wird ein Absatz
 *   2  Zwei Platzhalter in einer Zeile
 *   3  Unbekannter Platzhalter bleibt wörtlich stehen
 *   4  {{ ergibt ein wörtliches {
 *   5  Unabgeschlossene Klammer am Zeilenende bleibt stehen
 *   6  Leerer Wert ergibt leeren Text, nicht den Platzhalternamen
 *   7  Blockplatzhalter allein auf der Zeile erzeugt mehrere Zeilen
 *   8  Blockplatzhalter mitten im Text bleibt wörtlich
 *   9  Leerraum um den Blockplatzhalter ist erlaubt
 *  10  Block ohne Zeilen erzeugt keine Ausgabe
 *  11  Zu lange Zeile wird umgebrochen
 *  12  Umbruch am letzten Leerzeichen
 *  13  Umbruch zerreißt kein UTF-8-Zeichen
 *  14  Eine Ersetzung, die die Zeile zu lang macht, bricht um
 *  18  Rundlauf der Entwertung: [boot.rumpf]
 *  19  Rundlauf der Entwertung: \x
 *  20  Lesestück endet nie mitten in einem UTF-8-Zeichen
 *  26  "# " am Zeilenanfang wird eine Überschrift
 *  27  **fett** wird ausgezeichnet
 *  28  Einzelne Sterne bleiben Text
 *  29  [Text](http://...) wird ein Link
 *  30  Andere Adressschemata werden nicht verlinkt
 *  31  Spitze Klammern werden maskiert - auch im eingesetzten Wert
 *  32  Leerzeile erzeugt keine Ausgabe
 *  33  Umbruch zerreißt weder Tag noch Entität
 */

#include <unity.h>

#include <Arduino.h>

#include "utils/mail_template.h"

using namespace MailVorlage;

namespace {

/// Sammelt die ausgegebenen Zeilen.
struct Gesammelt {
  static constexpr size_t MAX = 16;
  char zeilen[MAX][ZEILE_MAX + 1];
  size_t anzahl{0};
};

void sammle(const char* text, size_t length, void* context) {
  Gesammelt* g = static_cast<Gesammelt*>(context);
  if (g->anzahl >= Gesammelt::MAX) {
    return;
  }
  const size_t nimm = length < ZEILE_MAX ? length : ZEILE_MAX;
  memcpy(g->zeilen[g->anzahl], text, nimm);
  g->zeilen[g->anzahl][nimm] = '\0';
  g->anzahl++;
}

const Paar WERTE[] = {{"geraet", "Frameclaw PS"}, {"ip", "172.17.1.44"}, {"ssid", "Magrathea"},
                      {"neustarts", "7"},         {"laufzeit", "2d 5h"}, {"datum", "31.08.2026"},
                      {"uhrzeit", "18:24"},       {"leer", ""}};

Umgebung umgebung(BlockGeber bloecke = nullptr, void* blockContext = nullptr) {
  Umgebung u;
  u.werte = WERTE;
  u.anzahl = sizeof(WERTE) / sizeof(WERTE[0]);
  u.bloecke = bloecke;
  u.blockContext = blockContext;
  return u;
}

/// Blockgeber, der drei feste Zeilen liefert - oder keine, wenn context sagt "leer".
void dreiZeilen(const char* name, ZeilenSenke aus, void* senkeContext, void* context) {
  const bool leer = context && *static_cast<bool*>(context);
  if (leer) {
    return;
  }
  char zeile[64];
  for (uint8_t i = 1; i <= 3; i++) {
    snprintf(zeile, sizeof(zeile), "<tr>%s %u</tr>", name, i);
    aus(zeile, strlen(zeile), senkeContext);
  }
}

Gesammelt expandiere(const char* zeile, const Umgebung& u) {
  Gesammelt g;
  expandiereZeile(zeile, u, sammle, &g);
  return g;
}

} // namespace

/// 1
void test_01_zeile_ohne_platzhalter() {
  Gesammelt g = expandiere("Hallo Welt", umgebung());
  TEST_ASSERT_EQUAL_UINT32(1, g.anzahl);
  TEST_ASSERT_EQUAL_STRING("<p>Hallo Welt</p>", g.zeilen[0]);
}

/// 2
void test_02_zwei_platzhalter_in_einer_zeile() {
  Gesammelt g = expandiere("{geraet} auf {ip}", umgebung());
  TEST_ASSERT_EQUAL_UINT32(1, g.anzahl);
  TEST_ASSERT_EQUAL_STRING("<p>Frameclaw PS auf 172.17.1.44</p>", g.zeilen[0]);
}

/// 3 - der Nutzer soll seinen Tippfehler in der Mail sehen, nicht eine Lücke
void test_03_unbekannter_platzhalter_bleibt_stehen() {
  Gesammelt g = expandiere("Wert: {quatsch}!", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>Wert: {quatsch}!</p>", g.zeilen[0]);
}

/// 4
void test_04_doppelte_klammer_ist_eine_klammer() {
  Gesammelt g = expandiere("{{geraet} bleibt", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>{geraet} bleibt</p>", g.zeilen[0]);

  Gesammelt h = expandiere("a {{ b", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>a { b</p>", h.zeilen[0]);
}

/// 5 - darf nicht über das Zeilenende hinauslesen
void test_05_unabgeschlossene_klammer() {
  Gesammelt g = expandiere("Ende: {geraet", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>Ende: {geraet</p>", g.zeilen[0]);
}

/// 6
void test_06_leerer_wert() {
  // Die eckige Klammer leitet auch einen Link ein; ohne folgendes ( bleibt sie Text.
  Gesammelt g = expandiere("[{leer}]", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>[]</p>", g.zeilen[0]);
}

/// 7
void test_07_block_allein_auf_der_zeile() {
  Gesammelt g = expandiere("{messwerte}", umgebung(dreiZeilen));
  TEST_ASSERT_EQUAL_UINT32(3, g.anzahl);
  TEST_ASSERT_EQUAL_STRING("<tr>messwerte 1</tr>", g.zeilen[0]);
  TEST_ASSERT_EQUAL_STRING("<tr>messwerte 3</tr>", g.zeilen[2]);
}

/// 8 - sonst müsste der Text davor und dahinter gepuffert werden
void test_08_block_mitten_im_text_bleibt_woertlich() {
  Gesammelt g = expandiere("Werte: {messwerte} soweit", umgebung(dreiZeilen));
  TEST_ASSERT_EQUAL_UINT32(1, g.anzahl);
  TEST_ASSERT_EQUAL_STRING("<p>Werte: {messwerte} soweit</p>", g.zeilen[0]);
}

/// 9 - Einrückung im HTML darf nicht stören
void test_09_leerraum_um_den_block() {
  Gesammelt g = expandiere("   {auffaellige}  ", umgebung(dreiZeilen));
  TEST_ASSERT_EQUAL_UINT32(3, g.anzahl);
  TEST_ASSERT_EQUAL_STRING("<tr>auffaellige 1</tr>", g.zeilen[0]);
}

/// 10
void test_10_block_ohne_zeilen() {
  bool leer = true;
  Gesammelt g = expandiere("{messwerte}", umgebung(dreiZeilen, &leer));
  TEST_ASSERT_EQUAL_UINT32(0, g.anzahl);
}

/// 11
void test_11_zu_lange_zeile_wird_umgebrochen() {
  char lang[400];
  memset(lang, 'a', sizeof(lang) - 1);
  lang[sizeof(lang) - 1] = '\0';

  Gesammelt g = expandiere(lang, umgebung());
  TEST_ASSERT_EQUAL_UINT32(2, g.anzahl);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(ZEILE_MAX, strlen(g.zeilen[0]));
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(ZEILE_MAX, strlen(g.zeilen[1]));
  // 399 Zeichen plus <p> und </p>
  TEST_ASSERT_EQUAL_UINT32(399 + 7, strlen(g.zeilen[0]) + strlen(g.zeilen[1]));
}

/// 12 - kein Wort zerreißen
void test_12_umbruch_am_leerzeichen() {
  char lang[350];
  memset(lang, 'x', sizeof(lang) - 1);
  lang[sizeof(lang) - 1] = '\0';
  lang[240] = ' ';

  // Das <p> davor verschiebt alles um drei Zeichen.
  Gesammelt g = expandiere(lang, umgebung());
  TEST_ASSERT_EQUAL_UINT32(2, g.anzahl);
  TEST_ASSERT_EQUAL_UINT32(244, strlen(g.zeilen[0]));
  TEST_ASSERT_EQUAL_CHAR(' ', g.zeilen[0][243]);
}

/// 13 - Emojis sind vier Byte lang; ein Schnitt mittendrin ergibt beim
/// Empfänger ein Ersatzzeichen
void test_13_umbruch_zerreisst_kein_utf8() {
  char lang[420] = {0};
  for (uint8_t i = 0; i < 100; i++) {
    memcpy(lang + i * 4, "\xF0\x9F\x9F\xA2", 4); // 🟢
  }

  Gesammelt g = expandiere(lang, umgebung());
  TEST_ASSERT_GREATER_THAN_UINT32(1, g.anzahl);
  for (size_t z = 0; z < g.anzahl; z++) {
    // Keine Zeile darf mit einem Folgebyte beginnen
    TEST_ASSERT_FALSE(istFolgeByte(g.zeilen[z][0]));
    // und keine mit einer angefangenen Sequenz enden
    const size_t len = strlen(g.zeilen[z]);
    TEST_ASSERT_EQUAL_UINT32(len, ganzeZeichen(g.zeilen[z], len));
  }
}

/// 14
void test_14_ersetzung_macht_die_zeile_zu_lang() {
  char langerName[300];
  memset(langerName, 'N', sizeof(langerName) - 1);
  langerName[sizeof(langerName) - 1] = '\0';
  const Paar werte[] = {{"geraet", langerName}};

  Umgebung u;
  u.werte = werte;
  u.anzahl = 1;

  Gesammelt g = expandiere("Name: {geraet}", u);
  TEST_ASSERT_GREATER_THAN_UINT32(1, g.anzahl);
  for (size_t z = 0; z < g.anzahl; z++) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(ZEILE_MAX, strlen(g.zeilen[z]));
  }
}

/// 26
void test_26_ueberschrift() {
  Gesammelt g = expandiere("# \xF0\x9F\x8C\xBB Hallo {geraet}", umgebung());
  TEST_ASSERT_EQUAL_UINT32(1, g.anzahl);
  TEST_ASSERT_EQUAL_STRING("<h1>\xF0\x9F\x8C\xBB Hallo Frameclaw PS</h1>", g.zeilen[0]);

  // Nur am Zeilenanfang und nur mit Leerzeichen dahinter
  Gesammelt h = expandiere("Nr. # 1", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>Nr. # 1</p>", h.zeilen[0]);

  Gesammelt i = expandiere("#kein Leerzeichen", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>#kein Leerzeichen</p>", i.zeilen[0]);
}

/// 27
void test_27_fett() {
  Gesammelt g = expandiere("Bei **{geraet}** klemmt es", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>Bei <strong>Frameclaw PS</strong> klemmt es</p>", g.zeilen[0]);
}

/// 28 - wer einen Stern tippt, soll einen Stern sehen
void test_28_einzelne_sterne_bleiben_text() {
  Gesammelt g = expandiere("3 * 4 = 12", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>3 * 4 = 12</p>", g.zeilen[0]);

  Gesammelt h = expandiere("**ohne Ende", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>**ohne Ende</p>", h.zeilen[0]);
}

/// 29
void test_29_link() {
  Gesammelt g = expandiere("[Jetzt nachschauen](http://{ip})", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p><a href=\"http://172.17.1.44\">Jetzt nachschauen</a></p>",
                           g.zeilen[0]);
}

/// 30 - aus einer Vorlage darf kein Mailprogramm etwas ausführen
void test_30_nur_erlaubte_adressen() {
  Gesammelt g = expandiere("[Klick](javascript:alert(1))", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>[Klick](javascript:alert(1))</p>", g.zeilen[0]);

  Gesammelt h = expandiere("[Schreib](mailto:tommy@fabmobil.org)", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p><a href=\"mailto:tommy@fabmobil.org\">Schreib</a></p>", h.zeilen[0]);
}

/// 31 - der Text ist kein HTML mehr, also darf auch keiner daraus werden
void test_31_maskierung() {
  Gesammelt g = expandiere("5 < 7 & \"eins\"", umgebung());
  TEST_ASSERT_EQUAL_STRING("<p>5 &lt; 7 &amp; &quot;eins&quot;</p>", g.zeilen[0]);

  const Paar boese[] = {{"geraet", "<script>x</script>"}};
  Umgebung u;
  u.werte = boese;
  u.anzahl = 1;
  Gesammelt h = expandiere("{geraet}", u);
  TEST_ASSERT_EQUAL_STRING("<p>&lt;script&gt;x&lt;/script&gt;</p>", h.zeilen[0]);
}

/// 32 - Leerzeilen gliedern den Editor, in der Mail macht der Stil die Abstände
void test_32_leerzeile() {
  Gesammelt g = expandiere("", umgebung());
  TEST_ASSERT_EQUAL_UINT32(0, g.anzahl);

  Gesammelt h = expandiere("   ", umgebung());
  TEST_ASSERT_EQUAL_UINT32(0, h.anzahl);
}

/// 33 - ein Umbruch mitten in <strong> oder in &amp; käme beim Empfänger als
/// Schrott an
void test_33_umbruch_zerreisst_kein_markup() {
  // Zeile aus lauter kaufmännischem Und: jedes wird zu fünf Zeichen
  char viele[120];
  memset(viele, '&', sizeof(viele) - 1);
  viele[sizeof(viele) - 1] = '\0';

  Gesammelt g = expandiere(viele, umgebung());
  TEST_ASSERT_GREATER_THAN_UINT32(1, g.anzahl);
  for (size_t z = 0; z < g.anzahl; z++) {
    // Keine Zeile darf mit einer angefangenen Entität enden
    const char* letztes = strrchr(g.zeilen[z], '&');
    if (letztes) {
      TEST_ASSERT_NOT_NULL(strchr(letztes, ';'));
    }
  }

  // Und direkt an der Grenze: ein Wort, das genau in den Umbruch fällt
  char lang[300];
  memset(lang, 'a', sizeof(lang) - 1);
  lang[sizeof(lang) - 1] = '\0';
  memcpy(lang + 250, "**fett**", 8);

  Gesammelt h = expandiere(lang, umgebung());
  for (size_t z = 0; z < h.anzahl; z++) {
    // Kein "<" ohne zugehöriges ">"
    const char* auf = strrchr(h.zeilen[z], '<');
    if (auf) {
      TEST_ASSERT_NOT_NULL(strchr(auf, '>'));
    }
  }
}

/// 20 - Wird eine überlange Zeile stückweise gelesen, darf der Schnitt nicht
/// mitten in eine UTF-8-Folge fallen: sonst stünde im Text ein Ersatzzeichen
/// und im nächsten Stück gleich noch eines.
void test_20_ganze_zeichen() {
  // Reines ASCII: nichts zu kürzen
  TEST_ASSERT_EQUAL_UINT32(5, ganzeZeichen("Hallo", 5));

  // Vollständiges Emoji am Ende bleibt
  const char* voll = "ab\xF0\x9F\x9F\xA2";
  TEST_ASSERT_EQUAL_UINT32(6, ganzeZeichen(voll, 6));

  // Angeschnittenes Emoji: auf die 2 davor kürzen
  TEST_ASSERT_EQUAL_UINT32(2, ganzeZeichen(voll, 5));
  TEST_ASSERT_EQUAL_UINT32(2, ganzeZeichen(voll, 4));
  TEST_ASSERT_EQUAL_UINT32(2, ganzeZeichen(voll, 3));

  // Zweibytezeichen (Umlaut)
  const char* umlaut = "gr\xC3\xBC"
                       "n";
  TEST_ASSERT_EQUAL_UINT32(5, ganzeZeichen(umlaut, 5));
  TEST_ASSERT_EQUAL_UINT32(2, ganzeZeichen(umlaut, 3));

  TEST_ASSERT_EQUAL_UINT32(0, ganzeZeichen(nullptr, 5));
  TEST_ASSERT_EQUAL_UINT32(0, ganzeZeichen("abc", 0));
}

// === Betreff ===

/// 21 - eine Kopfzeileneinschleusung wäre der einzige echte Angriffsweg hier
void test_21_betreff_verwirft_zeilenumbrueche() {
  char out[BETREFF_MAX + 1];
  expandiereBetreff("Warnung\r\nBcc: fremder@example.org", umgebung(), out, sizeof(out));
  TEST_ASSERT_NULL(strchr(out, '\r'));
  TEST_ASSERT_NULL(strchr(out, '\n'));
  TEST_ASSERT_EQUAL_STRING("WarnungBcc: fremder@example.org", out);
}

/// 22
void test_22_betreff_ohne_bloecke() {
  char out[BETREFF_MAX + 1];
  expandiereBetreff("{messwerte}", umgebung(dreiZeilen), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("{messwerte}", out);
}

void test_betreff_mit_platzhaltern() {
  char out[BETREFF_MAX + 1];
  const size_t n =
      expandiereBetreff("\xF0\x9F\x8C\xB1 {geraet} ist wach", umgebung(), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xF0\x9F\x8C\xB1 Frameclaw PS ist wach", out);
  TEST_ASSERT_EQUAL_UINT32(strlen(out), n);
}

// === Dateiformat ===

/// 15, 16, 17
void test_15_16_17_markenerkennung() {
  TEST_ASSERT_EQUAL(Abschnitt::BootRumpf, erkenneMarke("[boot.rumpf]"));
  TEST_ASSERT_EQUAL(Abschnitt::BootBetreff, erkenneMarke("[boot.betreff]"));
  TEST_ASSERT_EQUAL(Abschnitt::AliveRumpf, erkenneMarke("[alive.rumpf]"));

  // Großbuchstaben, Text davor, Leerraum dahinter: keine Marke
  TEST_ASSERT_EQUAL(Abschnitt::Keiner, erkenneMarke("[Boot.Rumpf]"));
  TEST_ASSERT_EQUAL(Abschnitt::Keiner, erkenneMarke("<p>[1]</p>"));
  TEST_ASSERT_EQUAL(Abschnitt::Keiner, erkenneMarke("[boot.rumpf] "));
  TEST_ASSERT_EQUAL(Abschnitt::Keiner, erkenneMarke("[]"));
  TEST_ASSERT_EQUAL(Abschnitt::Keiner, erkenneMarke(nullptr));

  // Sieht aus wie eine Marke, ist keine bekannte: wird überlesen statt abzubrechen
  TEST_ASSERT_EQUAL(Abschnitt::Unbekannt, erkenneMarke("[foo.bar]"));
}

/// 18, 19 - ohne Entwertung zerlegte eine HTML-Zeile wie "[1]" die Datei
void test_18_19_entwertung_rundlauf() {
  TEST_ASSERT_TRUE(brauchtEntwertung("[boot.rumpf]"));
  TEST_ASSERT_TRUE(brauchtEntwertung("\\x"));
  TEST_ASSERT_FALSE(brauchtEntwertung("<html>"));
  TEST_ASSERT_FALSE(brauchtEntwertung(""));

  TEST_ASSERT_EQUAL_STRING("[boot.rumpf]", entwerte("\\[boot.rumpf]"));
  TEST_ASSERT_EQUAL_STRING("\\x", entwerte("\\\\x"));
  TEST_ASSERT_EQUAL_STRING("<html>", entwerte("<html>"));

  // Eine entwertete Zeile darf nicht mehr als Marke gelten
  TEST_ASSERT_EQUAL(Abschnitt::Keiner, erkenneMarke("\\[boot.rumpf]"));
}

void test_bekannte_platzhalter() {
  TEST_ASSERT_TRUE(istBekannt("geraet", 6));
  TEST_ASSERT_TRUE(istBekannt("ssid", 4));
  TEST_ASSERT_TRUE(istBekannt("neustarts", 9));
  TEST_ASSERT_TRUE(istBekannt("messwerte", 9));
  TEST_ASSERT_TRUE(istBekannt("auffaellige", 11));
  TEST_ASSERT_FALSE(istBekannt("quatsch", 7));
  // Teiltreffer dürfen nicht zählen
  TEST_ASSERT_FALSE(istBekannt("ge", 2));

  TEST_ASSERT_TRUE(istBlock("messwerte", 9));
  TEST_ASSERT_FALSE(istBlock("geraet", 6));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_01_zeile_ohne_platzhalter);
  RUN_TEST(test_02_zwei_platzhalter_in_einer_zeile);
  RUN_TEST(test_03_unbekannter_platzhalter_bleibt_stehen);
  RUN_TEST(test_04_doppelte_klammer_ist_eine_klammer);
  RUN_TEST(test_05_unabgeschlossene_klammer);
  RUN_TEST(test_06_leerer_wert);
  RUN_TEST(test_07_block_allein_auf_der_zeile);
  RUN_TEST(test_08_block_mitten_im_text_bleibt_woertlich);
  RUN_TEST(test_09_leerraum_um_den_block);
  RUN_TEST(test_10_block_ohne_zeilen);
  RUN_TEST(test_11_zu_lange_zeile_wird_umgebrochen);
  RUN_TEST(test_12_umbruch_am_leerzeichen);
  RUN_TEST(test_13_umbruch_zerreisst_kein_utf8);
  RUN_TEST(test_14_ersetzung_macht_die_zeile_zu_lang);
  RUN_TEST(test_15_16_17_markenerkennung);
  RUN_TEST(test_18_19_entwertung_rundlauf);
  RUN_TEST(test_26_ueberschrift);
  RUN_TEST(test_27_fett);
  RUN_TEST(test_28_einzelne_sterne_bleiben_text);
  RUN_TEST(test_29_link);
  RUN_TEST(test_30_nur_erlaubte_adressen);
  RUN_TEST(test_31_maskierung);
  RUN_TEST(test_32_leerzeile);
  RUN_TEST(test_33_umbruch_zerreisst_kein_markup);
  RUN_TEST(test_20_ganze_zeichen);
  RUN_TEST(test_21_betreff_verwirft_zeilenumbrueche);
  RUN_TEST(test_22_betreff_ohne_bloecke);
  RUN_TEST(test_betreff_mit_platzhaltern);
  RUN_TEST(test_bekannte_platzhalter);
  return UNITY_END();
}
