/**
 * LED-Ampel Modul
 * Diese Datei enthält den Code für das LED-Ampel-Modul
 */


void LedampelBodenfeuchte(int bodenfeuchteMesswertProzent);
void LedampelHelligkeit(int helligkeitMesswertProzent);
/**
 * Funktion: LedampelBlinken(String farbe, int anzahl, int dauer)
 * Lässt die LED Ampel in einer Farbe blinken
 * farbe: String; "rot", "gruen" oder "gelb"
 * anzahl: Integer; Anzahl der Blinkvorgänge
 * dauer: Integer; Dauer eines Blinkvorganges in Millisekunden
 */
void LedampelBlinken(String farbe, int anzahl, int dauer) {
  #if MODUL_DEBUG
    Serial.println(F("# Beginn von LedampelBlinken()"));
    Serial.print(F("# Farbe: ")); Serial.print(farbe);
    Serial.print(F(", Anzahl: ")); Serial.print(anzahl);
    Serial.print(F(", Dauer: ")); Serial.println(dauer);
  #endif
  char PIN_LED;
  digitalWrite(ampelPinRot, LOW);
  digitalWrite(ampelPinGelb, LOW);
  digitalWrite(ampelPinGruen, LOW);
  if (farbe == "rot") { // wenn die Farbe rot ist, wird der Pin für die rote LED gesetzt
    PIN_LED = ampelPinRot;
  }
  if (farbe =="gelb") { // wenn die Farbe gelb ist, wird der Pin für die gelbe LED gesetzt
    PIN_LED = ampelPinGelb;
  }
  if (farbe == "gruen") { // wenn die Farbe grün ist, wird der Pin für die grüne LED gesetzt
    PIN_LED = ampelPinGruen;
  }
  for (int i=0;i<anzahl;i++){ // Schleife für die Anzahl der Blinkvorgänge
    digitalWrite(PIN_LED, HIGH); // LED an
    delay(dauer); // warten
    digitalWrite(PIN_LED, LOW); // LED aus
    delay(dauer); // warten
  }
}

/**
 * Funktion: LedampelAnzeigen(String farbe, int dauer)
 * Lässt die LED Ampel in einer Farbe leuchten
 * farbe: String; "rot", "gruen" oder "gelb"
 * dauer: Integer; Dauer der Farbanzeige in Millisekunden. Bei -1 bleibt die LED an.
 */
void LedampelAnzeigen(String farbe, int dauer) {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von LedampelAnzeigen(")); Serial.print(farbe);
    Serial.print(F(", ")); Serial.print(dauer);
    Serial.println(F(")"));
  #endif
  digitalWrite(ampelPinRot, LOW); // alle LEDs aus
  digitalWrite(ampelPinGelb, LOW);
  digitalWrite(ampelPinGruen, LOW);
  if (farbe == "rot") { // wenn die Farbe rot ist, wird der Pin für die rote LED gesetzt
    digitalWrite(ampelPinRot, HIGH);
    if (dauer != -1) { // wenn die Dauer nicht -1 ist, wird die LED nach der Dauer wieder ausgeschaltet
      delay(dauer);
      digitalWrite(ampelPinRot, LOW);
    }
  }
  if (farbe == "gelb") { // wenn die Farbe gelb ist, wird der Pin für die gelbe LED gesetzt
    digitalWrite(ampelPinGelb, HIGH);
    if (dauer != -1) { // wenn die Dauer nicht -1 ist, wird die LED nach der Dauer wieder ausgeschaltet
      delay(dauer);
      digitalWrite(ampelPinGelb, LOW);
    }
  }
  if (farbe == "gruen") { // wenn die Farbe grün ist, wird der Pin für die grüne LED gesetzt
    digitalWrite(ampelPinGruen, HIGH);
    if (dauer != -1) { // wenn die Dauer nicht -1 ist, wird die LED nach der Dauer wieder ausgeschaltet
      delay(dauer);
      digitalWrite(ampelPinGruen, LOW);
    }
  }
}

void LedampelUmschalten(int helligkeitMesswertProzent, int bodenfeuchteMesswertProzent) {
  /*
  * Falls es auch das Bodenfeuchte Modul gibt, blinkt die LED Ampel kurz 5x gelb damit
  * klar ist, dass jetzt die Lichtstärke angezeigt wird..
  */
  switch (ampelModus) { // je nach Modus wird die LED Ampel anders angesteuert

    case 0: // im Modus 0 (Helligkeits- und Bodensensor):
      ampelUmschalten = !ampelUmschalten; // wird hier invertiert. true: Helligkeitsanzeige, false: Bodenfeuchteanzeige
      if ( ampelUmschalten ) { // Wenn ampelUmschalten true ist:
        // LED Ampel blinkt 5x gelb um zu signalisieren, dass jetzt der Bodenfeuchtewert angezeigt wird
        if ( MODUL_BODENFEUCHTE ) { LedampelBlinken("gelb", 2, 500); } // Ampel blinkt nur, falls es auch ein Bodenfeuchtemodul gibt
        LedampelAnzeigen(bodenfeuchteFarbe, -1);
      } else { // Wenn ampelUmschalten false ist:
        if ( MODUL_HELLIGKEIT ) { LedampelBlinken("gruen", 2, 500); } // Ampel blinkt nur, falls es auch ein Helligkeitsmodul gibt
        LedampelAnzeigen(helligkeitFarbe, -1);
      }
      break; // Ende von Modus 0
    case 1: // Helligkeitsmodus
      LedampelAnzeigen(helligkeitFarbe, -1);
     // LedampelHelligkeit(helligkeitMesswertProzent); // Funktion LedampelHelligkeit() wird aufgerufen
      break; // Ende von Modus 1
    case 2: // Bodenfeuchtemodus
      LedampelAnzeigen(bodenfeuchteFarbe, -1);
      break; // Ende von bodenfeuchteFarbe 2
  }
}
