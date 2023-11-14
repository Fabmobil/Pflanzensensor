/**
 * LED-Ampel Modul
 * Diese Datei enthält den Code für das LED-Ampel-Modul
 */

/**
 * Funktion: LedampelBlinken(String farbe, int anzahl, int dauer)
 * Lässt die LED Ampel in einer Farbe blinken
 * farbe: String; "rot", "gruen" oder "gelb"
 * anzahl: Integer; Anzahl der Blinkvorgänge
 * dauer: Integer; Dauer eines Blinkvorganges in Millisekunden
 */
void LedampelBlinken(String farbe, int anzahl, int dauer) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von LedampelBlinken()"));
    Serial.print(F("Farbe: ")); Serial.print(farbe);
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
    Serial.println(F("## Debug: Beginn von LedampelAnzeigen(farbe, dauer)"));
    Serial.print  (F("Farbe: ")); Serial.print(farbe);
    Serial.print  (F(", Dauer: ")); Serial.println(dauer);
  #endif
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
