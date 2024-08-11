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
    variablen.putBool("bodenfWeb", bodenfeuchteWebhook);
    variablen.putString("bodenfName", bodenfeuchteName);
    variablen.putInt("bodenfGrUnten", bodenfeuchteGruenUnten);
    variablen.putInt("bodenfGrOben", bodenfeuchteGruenOben);
    variablen.putInt("bodenfGeUnten", bodenfeuchteGelbUnten);
    variablen.putInt("bodenfGeOben", bodenfeuchteGelbOben);
  #endif
  #if MODUL_ANALOG3
    variablen.putBool("analog3Web", analog3Webhook);
    variablen.putString("analog3Name", analog3Name);
    variablen.putInt("analog3GrUnten", analog3GruenUnten);
    variablen.putInt("analog3GrOben", analog3GruenOben);
    variablen.putInt("analog3GeUnten", analog3GelbUnten);
    variablen.putInt("analog3GeOben", analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    variablen.putBool("analog4Web", analog4Webhook);
    variablen.putString("analog4Name", analog4Name);
    variablen.putInt("analog4GrUnten", analog4GruenUnten);
    variablen.putInt("analog4GrOben", analog4GruenOben);
    variablen.putInt("analog4GeUnten", analog4GelbUnten);
    variablen.putInt("analog4GeOben", analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    variablen.putBool("analog5Web", analog5Webhook);
    variablen.putString("analog5Name", analog5Name);
    variablen.putInt("analog5GrUnten", analog5GruenUnten);
    variablen.putInt("analog5GrOben", analog5GruenOben);
    variablen.putInt("analog5GeUnten", analog5GelbUnten);
    variablen.putInt("analog5GeOben", analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    variablen.putBool("analog6Web", analog6Webhook);
    variablen.putString("analog6Name", analog6Name);
    variablen.putInt("analog6GrUnten", analog6GruenUnten);
    variablen.putInt("analog6GrOben", analog6GruenOben);
    variablen.putInt("analog6GeUnten", analog6GelbUnten);
    variablen.putInt("analog6GeOben", analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    variablen.putBool("analog7Web", analog7Webhook);
    variablen.putString("analog7Name", analog7Name);
    variablen.putInt("analog7GrUnten", analog7GruenUnten);
    variablen.putInt("analog7GrOben", analog7GruenOben);
    variablen.putInt("analog7GeUnten", analog7GelbUnten);
    variablen.putInt("analog7GeOben", analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    variablen.putBool("analog8Web", analog8Webhook);
    variablen.putString("analog8Name", analog8Name);
    variablen.putInt("analog8GrUnten", analog8GruenUnten);
    variablen.putInt("analog8GrOben", analog8GruenOben);
    variablen.putInt("analog8GeUnten", analog8GelbUnten);
    variablen.putInt("analog8GeOben", analog8GelbOben);
  #endif
  #if MODUL_DHT
    variablen.putInt("intDht", intervallDht);
    variablen.putBool("luftTWeb", lufttemperaturWebhook);
    variablen.putInt("luftTGrUnten", lufttemperaturGruenUnten);
    variablen.putInt("luftTGrOben", lufttemperaturGruenOben);
    variablen.putInt("luffTGeUnten", lufttemperaturGelbUnten);
    variablen.putInt("luftTGeOben", lufttemperaturGelbOben);
    variablen.putBool("luftFWeb", luftfeuchteWebhook);
    variablen.putInt("luftFGrUnten", luftfeuchteGruenUnten);
    variablen.putInt("luftFGrOben", luftfeuchteGruenOben);
    variablen.putInt("luftFGeUnten", luftfeuchteGelbUnten);
    variablen.putInt("luftFGeOben", luftfeuchteGelbOben);
  #endif
  #if MODUL_HELLIGKEIT
    variablen.putString("hellName", helligkeitName);
    variablen.putBool("hellWeb", helligkeitWebhook);
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
  #if MODUL_WEBHOOK
    variablen.putString("webhookPfad", webhookPfad);
    variablen.putString("webhookDomain", webhookDomain);
    variablen.putInt("webhookFrequenz", webhookFrequenz);
    variablen.putBool("webhookSchalter", webhookSchalter);
  #endif
  #if MODUL_WIFI
    variablen.putString("adminPw", wifiAdminPasswort);
    variablen.putString("hostname", wifiHostname);
    variablen.putBool("apAktiv", wifiAp);
    variablen.putString("apSsid", wifiApSsid);
    variablen.putBool("apPwAktiv", wifiApPasswortAktiviert);
    variablen.putString("apPw", wifiApPasswort);
    variablen.putString("wifiSsid1", wifiSsid1);
    variablen.putString("wifiPw1", wifiPassword1);
    variablen.putString("wifiSsid2", wifiSsid2);
    variablen.putString("wifiPw2", wifiPassword2);
    variablen.putString("wifiSsid3", wifiSsid3);
    variablen.putString("wifiPw3", wifiPassword3);
  #endif
  variablen.end();
}

// Function to load the variablen from flash
void VariablenLaden() {
  #if MODUL_DISPLAY // wenn das Display Modul aktiv ist:
    DisplayDreiWoerter("Start..", " Variablen", "  laden");
  #endif
  variablen.begin("pflanzensensor", true);

  // Load the variables from flash
  #if MODUL_DISPLAY
    intervallDisplay = variablen.getInt("intDisplay", intervallDisplay);
  #endif
  intervallAnalog = variablen.getInt("intAnalog", intervallAnalog);
  #if MODUL_BODENFEUCHTE
    bodenfeuchteWebhook = variablen.getInt("bodenfWeb", bodenfeuchteWebhook);
    bodenfeuchteGruenUnten = variablen.getInt("bodenfGrUnten", bodenfeuchteGruenUnten);
    bodenfeuchteGruenOben = variablen.getInt("bodenfGrOben", bodenfeuchteGruenOben);
    bodenfeuchteGelbUnten = variablen.getInt("bodenfGeUnten", bodenfeuchteGelbUnten);
    bodenfeuchteGelbOben = variablen.getInt("bodenfGeOben", bodenfeuchteGelbOben);
  #endif
  #if MODUL_ANALOG3
    analog3Name = variablen.getString("analog3Name", analog3Name);
    analog3Webhook = variablen.getInt("analog3Web", analog3Webhook);
    analog3GruenUnten = variablen.getInt("analog3GrUnten", analog3GruenUnten);
    analog3GruenOben = variablen.getInt("analog3GrOben", analog3GruenOben);
    analog3GelbUnten = variablen.getInt("analog3GeUnten", analog3GelbUnten);
    analog3GelbOben = variablen.getInt("analog3GeOben", analog3GelbOben);
  #endif
  #if MODUL_ANALOG4
    analog4Name = variablen.getString("analog4Name", analog4Name);
    analog4Webhook = variablen.getInt("analog4Web", analog4Webhook);
    analog4GruenUnten = variablen.getInt("analog4GrUnten", analog4GruenUnten);
    analog4GruenOben = variablen.getInt("analog4GrOben", analog4GruenOben);
    analog4GelbUnten = variablen.getInt("analog4GeUnten", analog4GelbUnten);
    analog4GelbOben = variablen.getInt("analog4GeOben", analog4GelbOben);
  #endif
  #if MODUL_ANALOG5
    analog5Name = variablen.getString("analog5Name", analog5Name);
    analog5Webhook = variablen.getInt("analog5Web", analog5Webhook);
    analog5GruenUnten = variablen.getInt("analog5GrUnten", analog5GruenUnten);
    analog5GruenOben = variablen.getInt("analog5GrOben", analog5GruenOben);
    analog5GelbUnten = variablen.getInt("analog5GeUnten", analog5GelbUnten);
    analog5GelbOben = variablen.getInt("analog5GeOben", analog5GelbOben);
  #endif
  #if MODUL_ANALOG6
    analog6Name = variablen.getString("analog6Name", analog6Name);
    analog6Webhook = variablen.getInt("analog6Web", analog6Webhook);
    analog6GruenUnten = variablen.getInt("analog6GrUnten", analog6GruenUnten);
    analog6GruenOben = variablen.getInt("analog6GrOben", analog6GruenOben);
    analog6GelbUnten = variablen.getInt("analog6GeUnten", analog6GelbUnten);
    analog6GelbOben = variablen.getInt("analog6GeOben", analog6GelbOben);
  #endif
  #if MODUL_ANALOG7
    analog7Name = variablen.getString("analog7Name", analog7Name);
    analog7Webhook = variablen.getInt("analog7Web", analog7Webhook);
    analog7GruenUnten = variablen.getInt("analog7GrUnten", analog7GruenUnten);
    analog7GruenOben = variablen.getInt("analog7GrOben", analog7GruenOben);
    analog7GelbUnten = variablen.getInt("analog7GeUnten", analog7GelbUnten);
    analog7GelbOben = variablen.getInt("analog7GeOben", analog7GelbOben);
  #endif
  #if MODUL_ANALOG8
    analog8Name = variablen.getString("analog8Name", analog8Name);
    analog8Webhook = variablen.getInt("analog8Web", analog8Webhook);
    analog8GruenUnten = variablen.getInt("analog8GrUnten", analog8GruenUnten);
    analog8GruenOben = variablen.getInt("analog8GrOben", analog8GruenOben);
    analog8GelbUnten = variablen.getInt("analog8GeUnten", analog8GelbUnten);
    analog8GelbOben = variablen.getInt("analog8GeOben", analog8GelbOben);
  #endif
  #if MODUL_DHT
    intervallDht = variablen.getInt("intDht", intervallDht);
    lufttemperaturWebhook = variablen.getInt("luftTWeb", lufttemperaturWebhook);
    lufttemperaturGruenUnten = variablen.getInt("luftTGrUnten", lufttemperaturGruenUnten);
    lufttemperaturGruenOben = variablen.getInt("luftTGrOben", lufttemperaturGruenOben);
    lufttemperaturGelbUnten = variablen.getInt("luffTGeUnten", lufttemperaturGelbUnten);
    lufttemperaturGelbOben = variablen.getInt("luftTGeOben", lufttemperaturGelbOben);
    luftfeuchteGruenUnten = variablen.getInt("luftFGrUnten", luftfeuchteGruenUnten);
    luftfeuchteWebhook = variablen.getInt("luftFWeb", luftfeuchteWebhook);
    luftfeuchteGruenOben = variablen.getInt("luftFGrOben", luftfeuchteGruenOben);
    luftfeuchteGelbUnten = variablen.getInt("luftFGeUnten", luftfeuchteGelbUnten);
    luftfeuchteGelbOben = variablen.getInt("luftFGeOben", luftfeuchteGelbOben);
  #endif
  #if MODUL_HELLIGKEIT
    helligkeitName = variablen.getString("hellName", helligkeitName);
    helligkeitWebhook = variablen.getInt("hellWeb", helligkeitWebhook);
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
  #if MODUL_WEBHOOK
    webhookPfad = variablen.getString("webhookPfad", webhookPfad).c_str();
    webhookDomain = variablen.getString("webhookDomain", webhookDomain).c_str();
    webhookSchalter = variablen.getBool("webhookSchalter", webhookSchalter);
    webhookFrequenz = variablen.getInt("webhookFrequenz", webhookFrequenz);
  #endif
  #if MODUL_WIFI
    wifiSsid1 = variablen.getString("wifiSsid1", wifiSsid1).c_str();
    wifiPassword1 = variablen.getString("wifiPw1", wifiPassword1).c_str();
    wifiSsid2 = variablen.getString("wifiSsid2", wifiSsid2).c_str();
    wifiPassword2 = variablen.getString("wifiPw2", wifiPassword2).c_str();
    wifiSsid3 = variablen.getString("wifiSsid3", wifiSsid3).c_str();
    wifiPassword3 = variablen.getString("wifiPw3", wifiPassword3).c_str();
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
