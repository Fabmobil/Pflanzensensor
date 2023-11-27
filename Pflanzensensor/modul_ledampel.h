/**
 * LED-Ampel Modul
 * Diese Datei enthält den Code für das LED-Ampel-Modul
 */


void LedampelBodenfeuchte(int messwertBodenfeuchteProzent);
void LedampelHelligkeit(int messwertHelligkeitProzent);
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
  digitalWrite(pinAmpelRot, LOW);
  digitalWrite(pinAmpelGelb, LOW);
  digitalWrite(pinAmpelGruen, LOW);
  if (farbe == "rot") {
    PIN_LED = pinAmpelRot;
  }
  if (farbe =="gelb") {
    PIN_LED = pinAmpelGelb;
  }
  if (farbe == "gruen") {
    PIN_LED = pinAmpelGruen;
  }
  for (int i=0;i<anzahl;i++){
    digitalWrite(PIN_LED, HIGH);
    delay(dauer);
    digitalWrite(PIN_LED, LOW);
    delay(dauer);
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
  digitalWrite(pinAmpelRot, LOW);
  digitalWrite(pinAmpelGelb, LOW);
  digitalWrite(pinAmpelGruen, LOW);
  if (farbe == "rot") {
    digitalWrite(pinAmpelRot, HIGH);
    if (dauer != -1) {
      delay(dauer);
      digitalWrite(pinAmpelRot, LOW);
    }
  }
  if (farbe == "gelb") {
    digitalWrite(pinAmpelGelb, HIGH);
    if (dauer != -1) {
      delay(dauer);
      digitalWrite(pinAmpelGelb, LOW);
    }
  }
  if (farbe == "gruen") {
    digitalWrite(pinAmpelGruen, HIGH);
    if (dauer != -1) {
      delay(dauer);
      digitalWrite(pinAmpelGruen, LOW);
    }
  }
}

void LedampelUmschalten(int messwertHelligkeitProzent, int messwertBodenfeuchteProzent) {
  /*
  * Falls es auch das Bodenfeuchte Modul gibt, blinkt die LED Ampel kurz 5x gelb damit
  * klar ist, dass jetzt die Lichtstärke angezeigt wird..
  */
  switch (ampelModus) {

    case 0: // im Modus 0 (Helligkeits- und Bodensensor):
      ampelUmschalten = !ampelUmschalten; // wird hier invertiert. true: Helligkeitsanzeige, false: Bodenfeuchteanzeige
      if ( ampelUmschalten ) {
        // LED Ampel blinkt 5x gelb um zu signalisieren, dass jetzt der Bodenfeuchtewert angezeigt wird
        if ( MODUL_BODENFEUCHTE ) { LedampelBlinken("gelb", 2, 500); } // Ampel blinkt nur, falls es auch ein Bodenfeuchtemodul gibt
        LedampelHelligkeit(messwertHelligkeitProzent);
      } else {
        if ( MODUL_HELLIGKEIT ) { LedampelBlinken("gruen", 2, 500); } // Ampel blinkt nur, falls es auch ein Helligkeitsmodul gibt
        LedampelBodenfeuchte(messwertBodenfeuchteProzent);
      }
      break;
    case 1: // Helligkeitsmodus
      LedampelHelligkeit(messwertHelligkeitProzent);
      break;
    case 2: // Bodenfeuchtemodus
      LedampelBodenfeuchte(messwertBodenfeuchteProzent);
      break;
  }
}

void LedampelBodenfeuchte(int messwertBodenfeuchteProzent) {
  // Anzeige der Bodenfeuchte:
  if ( !ampelUmschalten ) { // Wenn ampelUmschalten false ist:
    #if MODUL_DEBUG // Debugausgabe
      Serial.print(F("# ampelUmschalten:             "));
      Serial.print(ampelUmschalten);
      Serial.println(F(": Ledampel zeigt Bodenfeuchte an."));
      Serial.print(F("# ampelBodenfeuchteInvertiert: "));
      Serial.println(ampelBodenfeuchteInvertiert);
      Serial.print(F("# messwertBodenfeuchteProzent: "));
      Serial.println(messwertBodenfeuchteProzent);
      Serial.print(F("# ampelBodenfeuchteGruen:      "));
      Serial.print(ampelBodenfeuchteGruen);
      Serial.print(F(", ampelBodenfeuchteRot:        "));
      Serial.println(ampelBodenfeuchteRot);
    #endif
    if ( ampelBodenfeuchteInvertiert ) { // Bodenfeuchteskala invertiert:
      if ( messwertBodenfeuchteProzent >= ampelBodenfeuchteGruen ) { // zwischen 100% und gruenem Grenzwert
        LedampelAnzeigen("gruen", -1);
      }
      // zwischen gruenem und rotem Grenzwert:
      if ( (messwertBodenfeuchteProzent < ampelBodenfeuchteGruen) && (messwertBodenfeuchteProzent >= ampelBodenfeuchteRot) ) {
        LedampelAnzeigen("gelb", -1);
      }
      if ( messwertBodenfeuchteProzent < ampelBodenfeuchteRot ) { // zwischem rotem Grenzwert und 0%
        LedampelAnzeigen("rot", -1);
      }
    } else { // Bodenfeuchteskala nicht invertiert:
      if ( messwertBodenfeuchteProzent <= ampelBodenfeuchteGruen ) { // zwischen 0% und gruenem Grenzwert
        LedampelAnzeigen("gruen", -1);
      }
      // zwischen grünem und rotem Grenzwert:
      if ( (messwertBodenfeuchteProzent > ampelBodenfeuchteGruen ) && (messwertBodenfeuchteProzent <= ampelBodenfeuchteRot) ) {
        LedampelAnzeigen("gelb", -1);
      }
      if ( messwertBodenfeuchteProzent > ampelBodenfeuchteRot ) { // zwischen rotem Grenzwert und 100%
        LedampelAnzeigen("rot", -1);
      }
    }
  }
}

void LedampelHelligkeit(int messwertHelligkeitProzent) {
  // Anzeige der Helligkeit:
  #if MODUL_DEBUG // Debugausgabe
    Serial.print(F("# ampelModus:           "));
    Serial.println(ampelModus);
    Serial.print(F("# ampelUmschalten:           "));
    Serial.print(ampelUmschalten);
    Serial.println(F(": Ledampel zeigt Helligkeit an."));
    Serial.print(F("# ampelHelligkeitInvertiert: "));
    Serial.println(ampelHelligkeitInvertiert);
    Serial.print(F("# messwertHelligkeitProzent: "));
    Serial.println(messwertHelligkeitProzent);
    Serial.print(F("# ampelHelligkeitGruen: "));
    Serial.print(ampelHelligkeitGruen);
    Serial.print(F(", ampelHelligkeitRot: "));
    Serial.println(ampelHelligkeitRot);
  #endif
  // Unterscheidung, ob die Skala der Lichtstärke invertiert ist oder nicht:
  if ( ampelHelligkeitInvertiert ) { // Wenn ja, wird folgendermaßen unterschieden, ob rot, gelb oder grün angezeigt wird:
    if ( messwertHelligkeitProzent >= ampelHelligkeitGruen ) { // zwischen 100% und gruenem Grenzwert
      LedampelAnzeigen("gruen", -1);
    }
    // zwischen gruenem und rotem Grenzwert:
    if ( (messwertHelligkeitProzent < ampelHelligkeitGruen) && (messwertHelligkeitProzent >= ampelHelligkeitRot) ) {
      LedampelAnzeigen("gelb", -1);
    }
    if ( messwertHelligkeitProzent < ampelHelligkeitRot ) { // zwischem rotem Grenzwert und 0%
      LedampelAnzeigen("rot", -1);
    }
  } else { // Helligkeitsskala nicht invertiert
    if ( messwertHelligkeitProzent <= ampelHelligkeitGruen ) { // zwischen 0% und grünem Grenzwert
      LedampelAnzeigen("gruen", -1);
    }
    // zwischen grünem und roten Grenzwert
    if ( (messwertHelligkeitProzent <= ampelHelligkeitGruen) && (messwertHelligkeitProzent < ampelHelligkeitRot) ) {
      LedampelAnzeigen("gelb", -1);
    }
    if ( messwertHelligkeitProzent > ampelHelligkeitRot ) { // zwischen rotem Grenzwert und 100%
      LedampelAnzeigen("rot", -1);
    }
  }
}
