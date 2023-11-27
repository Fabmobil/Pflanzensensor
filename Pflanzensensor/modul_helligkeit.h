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
    Serial.println(F("# Beginn von HelligkeitMessen()"));
  #endif
  // Helligkeit messen
  int messwertHelligkeit = analogRead(pinAnalog); // Messwert des Helligkeitssensors
  return messwertHelligkeit; // Messwert zurückgeben
}

/*
 * Funktion: HelligkeitUmrechnen(int helligkeit, int helligkeitMinimum, int helligkeitMaximum)
 * Macht aus dem analogen Messwert helligkeit einen Prozentwert, in dem er den Messwert auf eine
 * Skala zwischen helligkeitMinimum und helligkeitMaximum mappt
 */
int HelligkeitUmrechnen(int helligkeit, int helligkeitMinimum, int helligkeitMaximum) {
  #if MODUL_DEBUG
    Serial.print(F("# Beginn von HelligkeitUmrechnen(")); Serial.print(helligkeit);
    Serial.print(F(", ")); Serial.print(helligkeitMinimum);
    Serial.print(F(", ")); Serial.print(helligkeitMaximum);
    Serial.println(F(")"));
  #endif
  // Convert MIN reading (100) -> MAX reading (700) to a range 0->100.
  messwertHelligkeitProzent = map(helligkeit, helligkeitMinimum, helligkeitMaximum, 0, 100); // Umrechnung in Prozent
  Serial.print(F("Helligkeit: ")); Serial.print(messwertHelligkeitProzent);
  Serial.print(F("%         (Messwert: ")); Serial.print(messwertHelligkeit);
  Serial.println(F(")"));
  return messwertHelligkeitProzent;   // Prozentwert zurückgeben
}
