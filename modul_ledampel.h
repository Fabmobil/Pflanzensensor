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
    Serial.print(F("Farbe: ")); Serial.println(farbe);
    Serial.print(F("Anzahl: ")); Serial.println(anzahl);
    Serial.print(F("Dauer: ")); Serial.println(dauer);
    Serial.println(F("#######################################"));
  #endif
  char PIN_LED;
  digitalWrite(PIN_LEDAMPEL_ROTELED, LOW);
  digitalWrite(PIN_LEDAMPEL_GELBELED, LOW);
  digitalWrite(PIN_LEDAMPEL_GRUENELED, LOW);
  if (farbe == "rot") {
    PIN_LED = PIN_LEDAMPEL_ROTELED;
  } 
  if (farbe =="gelb") {
    PIN_LED = PIN_LEDAMPEL_GELBELED;
  } 
  if (farbe == "gruen") {
    PIN_LED = PIN_LEDAMPEL_GRUENELED;
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
    Serial.print  (F("Farbe: ")); Serial.println(farbe);
    Serial.print  (F("Dauer: ")); Serial.println(dauer);
    Serial.println(F("#######################################"));
  #endif
  if (farbe == "rot") {
    digitalWrite(PIN_LEDAMPEL_ROTELED, HIGH);
    if (dauer != -1) {
      delay(dauer);
      digitalWrite(PIN_LEDAMPEL_ROTELED, LOW);
    }
  }
  if (farbe == "gelb") {
    digitalWrite(PIN_LEDAMPEL_GELBELED, HIGH);
    if (dauer != -1) {
      delay(dauer);
      digitalWrite(PIN_LEDAMPEL_GELBELED, LOW);
    }
  } 
  if (farbe == "gruen") {
    digitalWrite(PIN_LEDAMPEL_GRUENELED, HIGH);
    if (dauer != -1) {
      delay(dauer);
      digitalWrite(PIN_LEDAMPEL_GRUENELED, LOW);
    }
  }  
}
