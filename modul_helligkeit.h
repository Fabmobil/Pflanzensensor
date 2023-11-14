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
  // Helligkeit messen
  int messwertHelligkeit = analogRead(pinAnalog);
  #if MODUL_DEBUG
    Serial.print  (F("Helligkeit absolut: ")); Serial.println(messwertHelligkeit);
  #endif
  return messwertHelligkeit;
}

/*
 * Funktion: HelligkeitUmrechnen(int helligkeit, int helligkeitMinimum, int helligkeitMaximum)
 * Macht aus dem analogen Messwert helligkeit einen Prozentwert, in dem er den Messwert auf eine
 * Skala zwischen helligkeitMinimum und helligkeitMaximum mappt
 */
int HelligkeitUmrechnen(int helligkeit, int helligkeitMinimum, int helligkeitMaximum) {
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von HelligkeitUmrechnen()"));
  #endif
  // Convert MIN reading (100) -> MAX reading (700) to a range 0->100.
  messwertHelligkeitProzent = map(helligkeit, helligkeitMinimum, helligkeitMaximum, 0, 100);
  #if MODUL_DEBUG
    Serial.print  (F("Helligkeit Prozent: ")); Serial.println(messwertHelligkeitProzent);
  #endif
   return messwertHelligkeitProzent;
}
