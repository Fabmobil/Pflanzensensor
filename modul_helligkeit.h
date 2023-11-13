/**
 * Helligkeitssensor-Modul
 * Diese Datei enthält den Code für den analogen Helligkeitssensor
 */

/*
 * Funktion: HelligkeitMessen()
 *  Misst den Analogwert des Lichtsensors
 */
int HelligkeitMessen() {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von HelligkeitMessen()"));
  #endif
  // Lichtstaerke messen
  int messwertHelligkeit = analogRead(pinAnalog);
  Serial.print("Messwert Lichtstärke: ");
  Serial.println(messwertHelligkeit);
  #if MODUL_DEBUG
    Serial.print  (F("Lichtstärke absolut: ")); Serial.println(messwertHelligkeit);
    Serial.println(F("#######################################"));
  #endif
  return messwertHelligkeit;
}

/*
 * Funktion: HelligkeitUmrechnen(int lichtstaerke, int lichtstaerkeMinimum, int lichtstaerkeMaximum)
 * Macht aus dem analogen Messwert lichtstaerke einen Prozentwert, in dem er den Messwert auf eine
 * Skala zwischen lichtstaerkeMinimum und lichtstaerkeMaximum mappt
 */
int HelligkeitUmrechnen(int lichtstaerke, int lichtstaerkeMinimum, int lichtstaerkeMaximum) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von HelligkeitUmrechnen()"));
    Serial.println(F("#######################################"));
  #endif
  // Convert MIN reading (100) -> MAX reading (700) to a range 0->100.
   int messwertHelligkeitProzent = map(lichtstaerke, lichtstaerkeMinimum, lichtstaerkeMaximum, 0, 100);
  #if MODUL_DEBUG
    Serial.print  (F("Lichtstärke %: ")); Serial.println(messwertHelligkeitProzent);
    Serial.println(F("#######################################"));
  #endif
   return messwertHelligkeitProzent;
}
