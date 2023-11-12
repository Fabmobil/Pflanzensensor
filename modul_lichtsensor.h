/**
 * LED-Ampel Modul
 * Diese Datei enthält den Code für das LED-Ampel-Modul
 */


/*
 * Funktion: LichtsensorMessen()
 *  Misst den Analogwert des Lichtsensors
 */
int LichtsensorMessen() {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von LichtsensorMessen()"));
  #endif
  // Lichtstaerke messen
  int LICHTSTAERKE_MESSWERT = analogRead(PIN_ANALOG);
  Serial.print("Messwert Lichtstärke: ");
  Serial.println(LICHTSTAERKE_MESSWERT); 
  #if MODUL_DEBUG
    Serial.print  (F("Lichtstärke absolut: ")); Serial.println(LICHTSTAERKE_MESSWERT);
    Serial.println(F("#######################################"));
  #endif
  return LICHTSTAERKE_MESSWERT;
}

/* 
 * Funktion: LichtsensorUmrechnen(int lichtstaerke, int VAR_LICHTSTAERKE_MIN, int VAR_LICHTSTAERKE_MAX)
 * Macht aus dem analogen Messwert lichtstaerke einen Prozentwert, in dem er den Messwert auf eine
 * Skala zwischen VAR_LICHTSTAERKE_MIN und VAR_LICHTSTAERKE_MAX mappt
 */
int LichtsensorUmrechnen(int lichtstaerke, int VAR_LICHTSTAERKE_MIN, int VAR_LICHTSTAERKE_MAX) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von LichtsensorUmrechnen()"));
    Serial.println(F("#######################################"));
  #endif
  // Convert MIN reading (100) -> MAX reading (700) to a range 0->100.
   int LICHTSTAERKE_PROZENT = map(lichtstaerke, VAR_LICHTSTAERKE_MIN, VAR_LICHTSTAERKE_MAX, 0, 100);
  #if MODUL_DEBUG
    Serial.print  (F("Lichtstärke %: ")); Serial.println(LICHTSTAERKE_PROZENT);
    Serial.println(F("#######################################"));
  #endif
   return LICHTSTAERKE_PROZENT;
}
