#include <Preferences.h>

Preferences variablen;

// Diese Funktion überprüft, ob das bool variablenDa auf dem Flash vorhanden ist
bool VariablenDa() {
  variablen.begin("pflanzensensor", true);
  bool variablenDa = variablen.getBool("variablenDa", false);
  return variablenDa;
}

void VariablenSpeichern() {
  variablen.begin("pflanzensensor", false);

  // Save the variablen to flash
  variablen.putBool("variablenDa", true);

  // Save the variables to flash
  #if MODUL_DISPLAY
    variablen.putInt("intDisplay", intervallDisplay);
  #endif
  variablen.putInt("intBodenf", intervallAnalog);
  #if MODUL_BODENFEUCHTE
    variablen.putString("bodenfName", bodenfeuchteName);
    variablen.putInt("bodenfGrUnten", bodenfeuchteGruenUnten);
    variablen.putInt("bodenfGrOben", bodenfeuchteGruenOben);
    variablen.putInt("bodenfGeUnten", bodenfeuchteGelbUnten);
    variablen.putInt("bodenfGeOben", bodenfeuchteGelbOben);
  #endif
  #if MODUL_ANALOG3
    variablen.putString("analog3Name", analog3Name);
    variablen.putInt("analog3GrUnten", analog3GruenUnten);
    variablen.putInt("analog3GrOben", analog3GruenOben);
    variablen.putInt("analog3GeUnten", analog3GelbUnten);
    variablen.putInt("analog3GeOben", analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    variablen.putString("analog4Name", analog4Name);
    variablen.putInt("analog4GrUnten", analog4GruenUnten);
    variablen.putInt("analog4GrOben", analog4GruenOben);
    variablen.putInt("analog4GeUnten", analog4GelbUnten);
    variablen.putInt("analog4GeOben", analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    variablen.putString("analog5Name", analog5Name);
    variablen.putInt("analog5GrUnten", analog5GruenUnten);
    variablen.putInt("analog5GrOben", analog5GruenOben);
    variablen.putInt("analog5GeUnten", analog5GelbUnten);
    variablen.putInt("analog5GeOben", analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    variablen.putString("analog6Name", analog6Name);
    variablen.putInt("analog6GrUnten", analog6GruenUnten);
    variablen.putInt("analog6GrOben", analog6GruenOben);
    variablen.putInt("analog6GeUnten", analog6GelbUnten);
    variablen.putInt("analog6GeOben", analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    variablen.putString("analog7Name", analog7Name);
    variablen.putInt("analog7GrUnten", analog7GruenUnten);
    variablen.putInt("analog7GrOben", analog7GruenOben);
    variablen.putInt("analog7GeUnten", analog7GelbUnten);
    variablen.putInt("analog7GeOben", analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    variablen.putString("analog8Name", analog8Name);
    variablen.putInt("analog8GrUnten", analog8GruenUnten);
    variablen.putInt("analog8GrOben", analog8GruenOben);
    variablen.putInt("analog8GeUnten", analog8GelbUnten);
    variablen.putInt("analog8GeOben", analog8GelbOben);
  #endif
  #if MODUL_DHT
    variablen.putInt("intDht", intervallDht);
    variablen.putInt("luftTGrUnten", lufttemperaturGruenUnten);
    variablen.putInt("luftTGrOben", lufttemperaturGruenOben);
    variablen.putInt("luffTGeUnten", lufttemperaturGelbUnten);
    variablen.putInt("luftTGeOben", lufttemperaturGelbOben);
    variablen.putInt("luftFGrUnten", luftfeuchteGruenUnten);
    variablen.putInt("luftFGrOben", luftfeuchteGruenOben);
    variablen.putInt("luftFGeUnten", luftfeuchteGelbUnten);
    variablen.putInt("luftFGeOben", luftfeuchteGelbOben);
  #endif
  #if MODUL_HELLIGKEIT
    variablen.putString("hellName", helligkeitName);
    variablen.putInt("hellMin", helligkeitMinimum);
    variablen.putInt("hellMax", helligkeitMaximum);
    variablen.putInt("hellGrUnten", helligkeitGruenUnten);
    variablen.putInt("hellGrOben", helligkeitGruenOben);
    variablen.putInt("hellGeUnten", helligkeitGelbUnten);
    variablen.putInt("hellGeOben", helligkeitGelbOben);
  #endif
  #if MODUL_LEDAMPEL
    variablen.putInt("ampelModus", ampelModus);
    variablen.putInt("intAmpel", intervallAmpel);
  #endif
  #if MODUL_IFTTT
    variablen.putString("iftttPw", wifiIftttPasswort);
    variablen.putString("iftttEreignis", wifiIftttEreignis);
  #endif
  #if MODUL_WIFI
    variablen.putString("adminPw", wifiAdminPasswort);
    variablen.putString("hostname", wifiHostname);
    variablen.putBool("apAktiv", wifiAp);
    variablen.putString("apSsid", wifiApSsid);
    variablen.putBool("apPwAktiv", wifiApPasswortAktiviert);
    variablen.putString("apPw", wifiApPasswort);
    variablen.putString("wifiSsid", wifiSsid);
    variablen.putString("wifiPw", wifiPassword);
  #endif
  variablen.end();
}

// Function to load the variablen from flash
void VariablenLaden() {
  variablen.begin("pflanzensensor", true);

  // Load the variables from flash
  #if MODUL_DISPLAY
    intervallDisplay = variablen.getInt("intDisplay", intervallDisplay);
  #endif
  intervallAnalog = variablen.getInt("intAnalog", intervallAnalog);
  #if MODUL_BODENFEUCHTE
    bodenfeuchteGruenUnten = variablen.getInt("bodenfGrUnten", bodenfeuchteGruenUnten);
    bodenfeuchteGruenOben = variablen.getInt("bodenfGrOben", bodenfeuchteGruenOben);
    bodenfeuchteGelbUnten = variablen.getInt("bodenfGeUnten", bodenfeuchteGelbUnten);
    bodenfeuchteGelbOben = variablen.getInt("bodenfGeOben", bodenfeuchteGelbOben);
  #endif
  #if MODUL_ANALOG3
    analog3Name = variablen.getString("analog3Name", analog3Name);
    analog3GruenUnten = variablen.getInt("analog3GrUnten", analog3GruenUnten);
    analog3GruenOben = variablen.getInt("analog3GrOben", analog3GruenOben);
    analog3GelbUnten = variablen.getInt("analog3GeUnten", analog3GelbUnten);
    analog3GelbOben = variablen.getInt("analog3GeOben", analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    analog4Name = variablen.getString("analog4Name", analog4Name);
    analog4GruenUnten = variablen.getInt("analog4GrUnten", analog4GruenUnten);
    analog4GruenOben = variablen.getInt("analog4GrOben", analog4GruenOben);
    analog4GelbUnten = variablen.getInt("analog4GeUnten", analog4GelbUnten);
    analog4GelbOben = variablen.getInt("analog4GeOben", analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    analog5Name = variablen.getString("analog5Name", analog5Name);
    analog5GruenUnten = variablen.getInt("analog5GrUnten", analog5GruenUnten);
    analog5GruenOben = variablen.getInt("analog5GrOben", analog5GruenOben);
    analog5GelbUnten = variablen.getInt("analog5GeUnten", analog5GelbUnten);
    analog5GelbOben = variablen.getInt("analog5GeOben", analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    analog6Name = variablen.getString("analog6Name", analog6Name);
    analog6GruenUnten = variablen.getInt("analog6GrUnten", analog6GruenUnten);
    analog6GruenOben = variablen.getInt("analog6GrOben", analog6GruenOben);
    analog6GelbUnten = variablen.getInt("analog6GeUnten", analog6GelbUnten);
    analog6GelbOben = variablen.getInt("analog6GeOben", analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    analog7Name = variablen.getString("analog7Name", analog7Name);
    analog7GruenUnten = variablen.getInt("analog7GrUnten", analog7GruenUnten);
    analog7GruenOben = variablen.getInt("analog7GrOben", analog7GruenOben);
    analog7GelbUnten = variablen.getInt("analog7GeUnten", analog7GelbUnten);
    analog7GelbOben = variablen.getInt("analog7GeOben", analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    analog8Name = variablen.getString("analog8Name", analog8Name);
    analog8GruenUnten = variablen.getInt("analog8GrUnten", analog8GruenUnten);
    analog8GruenOben = variablen.getInt("analog8GrOben", analog8GruenOben);
    analog8GelbUnten = variablen.getInt("analog8GeUnten", analog8GelbUnten);
    analog8GelbOben = variablen.getInt("analog8GeOben", analog8GelbOben);
  #endif
  #if MODUL_DHT
    intervallDht = variablen.getInt("intDht", intervallDht);
    lufttemperaturGruenUnten = variablen.getInt("luftTGrUnten", lufttemperaturGruenUnten);
    lufttemperaturGruenOben = variablen.getInt("luftTGrOben", lufttemperaturGruenOben);
    lufttemperaturGelbUnten = variablen.getInt("luffTGeUnten", lufttemperaturGelbUnten);
    lufttemperaturGelbOben = variablen.getInt("luftTGeOben", lufttemperaturGelbOben);
    luftfeuchteGruenUnten = variablen.getInt("luftFGrUnten", luftfeuchteGruenUnten);
    luftfeuchteGruenOben = variablen.getInt("luftFGrOben", luftfeuchteGruenOben);
    luftfeuchteGelbUnten = variablen.getInt("luftFGeUnten", luftfeuchteGelbUnten);
    luftfeuchteGelbOben = variablen.getInt("luftFGeOben", luftfeuchteGelbOben);
  #endif
  #if MODUL_HELLIGKEIT
    helligkeitName = variablen.getString("hellName", helligkeitName);
    helligkeitMinimum = variablen.getInt("hellMin", helligkeitMinimum);
    helligkeitMaximum = variablen.getInt("hellMax", helligkeitMaximum);
    helligkeitGruenUnten = variablen.getInt("hellGrUnten", helligkeitGruenUnten);
    helligkeitGruenOben = variablen.getInt("hellGrOben", helligkeitGruenOben);
    helligkeitGelbUnten = variablen.getInt("hellGeUnten", helligkeitGelbUnten);
    helligkeitGelbOben = variablen.getInt("hellGeOben", helligkeitGelbOben);
  #endif
  #if MODUL_LEDAMPEL
    ampelModus = variablen.getInt("ampelModus", ampelModus);
    intervallAmpel = variablen.getInt("intAmpel", intervallAmpel);
  #endif
  #if MODUL_IFTTT
    wifiIftttPasswort = variablen.getString("iftttPw", wifiIftttPasswort);
    wifiIftttEreignis = variablen.getString("iftttEreignis", wifiIftttEreignis);
  #endif
  #if MODUL_WIFI
    wifiAdminPasswort = variablen.getString("adminPw", wifiAdminPasswort);
    wifiHostname = variablen.getString("hostname", wifiHostname);
    wifiAp = variablen.getBool("apAktiv", wifiAp);
    wifiApSsid = variablen.getString("apSsid", wifiApSsid);
    wifiApPasswortAktiviert = variablen.getBool("apPwAktiv", wifiApPasswortAktiviert);
    wifiApPasswort = variablen.getString("apPw", wifiApPasswort);
    wifiSsid = variablen.getString("wifiSsid", wifiSsid);
    wifiPassword = variablen.getString("wifiPw", wifiPassword);
  #endif
  variablen.end();
}

void VariablenLoeschen() {
  if (VariablenDa() == true) {
    variablen.begin("pflanzensensor", false);
    variablen.clear();
    variablen.end();;
  };
}
