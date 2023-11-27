/**
 * Bodenfeuchte Modul
 * Diese Datei enthält den Code für das Bodenfeuchte-Modul
 */


/**
 * Funktion: BodenfeuchteUmrechnen(int bodenfeuchte, int bodenfeuchteMinimum, int bodenfeuchteMaximum)
 * Rechnet den Bodenfeuchte-Analogmesswert in eine Prozentzahl um und gibt diese als Integer zurück
 * messwert: Analogmesswert der Bodenfeuchte
 */
int BodenfeuchteUmrechnen(int bodenfeuchte, int bodenfeuchteMinimum, int bodenfeuchteMaximum) {
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
    Serial.print(F("# Beginn von BodenfeuchteUmrechnen("));
    Serial.print(bodenfeuchte);
    Serial.print(F(", "));
    Serial.print(bodenfeuchteMinimum);
    Serial.print(F(", "));
    Serial.print(bodenfeuchteMaximum);
    Serial.println(F(")"));
  #endif

  // Convert MIN reading (100) -> MAX reading (700) to a range 0->100.
  messwertBodenfeuchteProzent = map(bodenfeuchte, bodenfeuchteMinimum, bodenfeuchteMaximum, 0, 100);
  Serial.print(F("bodenfeuchte: ")); Serial.print(messwertBodenfeuchteProzent);
  Serial.print(F("%       (Messwert: ")); Serial.print(messwertBodenfeuchte);
  Serial.println(F(")"));
  return messwertBodenfeuchteProzent;
}

