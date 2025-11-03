# ![Fabmobil Logo](https://github.com/Fabmobil/Pflanzensensor/blob/main/Dokumentation/Bilder/Fabmobil_Logo.png?raw=true) Pflanzensensor

Dies ist das Repository mit dem Quellcode und allen Informationen zum Fabmobil Pflanzensensor. Weiterführende Erklärungen sind im [Wiki](https://github.com/Fabmobil/Pflanzensensor/wiki) zu finden.

Der Quellcode befindet sich im [Pflanzensensor](https://github.com/Fabmobil/Pflanzensensor/tree/main/Pflanzensensor)-Verzeichnis. Das [Dokumentation](https://github.com/Fabmobil/Pflanzensensor/tree/main/Dokumentation)-Verzeichnis enthält Datenblätter, Schaltpläne und Pinouts der verwendeten Bauteile sowie 3D Modelle für Zubehörteile, die gedruckt werden können. Im [Wiki](https://github.com/Fabmobil/Pflanzensensor/wiki) gibt es weiterführende Informationen und Anleitungen.

## Konfigurationsverwaltung

Das Projekt verwendet eine Preferences-basierte Konfigurationsverwaltung für eine robuste und effiziente Speicherung von Einstellungen. Weitere Details finden Sie in der [Preferences-Dokumentation](PREFERENCES_DOCUMENTATION.md).

**Neue Features:**
- EEPROM-basierte Speicherung (überlebt Filesystem-Flashes)
- Organisierte Namespace-Struktur für verschiedene Einstellungsbereiche
- Direktes Laden/Speichern in Preferences (keine JSON-Migration)
- Umfassende deutsche Logger-Ausgaben

**Aktueller Stand:**
- ✅ General, WiFi, Display, Log, LED Traffic Light und Debug-Einstellungen in Preferences
- ⏳ Sensor-Einstellungen noch in sensors.json (zukünftige Erweiterung)

![Pflanzensensor](https://github.com/Fabmobil/Pflanzensensor/blob/main/Dokumentation/Bilder/Pflanzensensor.jpeg?raw=true)
![Pflanzensensor in Blumentopf](https://github.com/Fabmobil/Pflanzensensor/blob/main/Dokumentation/Bilder/Pflanzensensor_in_Blumentopf.jpeg?raw=true)
![Pflanzensensor Webinterface](https://github.com/Fabmobil/Pflanzensensor/blob/main/Dokumentation/Bilder/Webinterface.png?raw=true)

[<img height="50" alt="Bosch" src="https://github.com/user-attachments/assets/16b5bcbe-fffa-43eb-bbd5-1f107192353f" />](https://www.fabmobil.org) 

wird gefördert von:

[<img height="100" alt="smk_logo" src="https://github.com/user-attachments/assets/7ffa939d-7818-4493-9b44-f4c17383e565" />](https://www.smk.sachsen.de/)


und unterstützt von:

[<img height="50" alt="sisax" src="https://github.com/user-attachments/assets/6c537384-fd33-47a8-a5d2-1312b83e6d65" />](https://silicon-saxony.de) [<img height="50" alt="Bosch" src="https://github.com/user-attachments/assets/1608d210-c723-4fde-8562-5f9e31beae52" />](https://www.bosch.de/) [<img height="50" alt="GlobalFoundries" src="https://github.com/user-attachments/assets/e2cb9d59-d5e6-4c50-9c5f-e5e620ed73a0" />](https://gf.com/) [<img height="50" alt="Bosch" src="https://github.com/user-attachments/assets/1c521a03-35c7-4104-9768-8d68bf8f7880" />](https://www.infineon.com/) [<img height="50" alt="Bosch" src="https://github.com/user-attachments/assets/b8cbed31-5312-494d-bc99-1a30388150f3" />](https://www.xfab.com/)
