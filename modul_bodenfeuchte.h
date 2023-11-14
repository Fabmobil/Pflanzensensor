/**
 * Bodenfeuchte Modul
 * Diese Datei enthält den Code für das Bodenfeuchte-Modul
 */


/**
 * Funktion: BodenfeuchteUmrechnung(int messwert)
 * Rechnet den Bodenfeuchte-Analogmesswert in eine Prozentzahl um und gibt diese als Integer zurück
 * messwert: Analogmesswert der Bodenfeuchte
 */
int BodenfeuchteUmrechnung(int messwert) {
  /* Es wird von einem 10 Bit Wert ausgegangen (ESP8266)
     0 entstricht 100%
     1023 entspricht 0% ESP8266 (10 Bit)
     Dazwischen wird linear umgerechnet.
     Das ist nicht präzise, genügt aber für den
     Anwendungsfall Bodenfeuchtemessung
     Bislang keinen Wert über 80 und unter 20 beobachtet, daher Spreizung der Skala
     20 und kleiner -> 0%
     80 und größer -> 100%
  */
  #if MODUL_DEBUG
    Serial.println(F("## Debug: Beginn von BodenfeuchteUmrechnung(messwert)"));
  #endif
  float quotient=1-(float)messwert/1023.0; // ESP8266
  float skaliert=(quotient*100.0-50.0)*(100.0/(77.0-50.0));
  int prozent=(int)(skaliert+0.5);
  #if MODUL_DEBUG
    Serial.print(F("Bodenfeuchte Messwert: ")); Serial.println(messwert);
    Serial.print(F("Bodenfeuchte transformiert: ")); Serial.println(skaliert);
    Serial.print(F("Bodenfeuchte Prozent: ")); Serial.println(prozent);
  #endif
  return prozent;
}
