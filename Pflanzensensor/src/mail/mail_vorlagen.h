/**
 * @file mail_vorlagen.h
 * @brief Mailvorlagen von Flash oder aus dem PROGMEM, zeilenweise
 * @details Die Vorlagen liegen in /config/mailvorlagen.txt. Gelesen wird immer
 *          zeilenweise und sofort weitergereicht - der Rumpf liegt nie ganz im
 *          Heap. Das ist keine Feinheit: der Versand läuft mit rund 15 KB
 *          freiem Speicher, und der TLS-Handshake braucht davon schon 10,7 KB.
 *
 *          Fehlt die Datei, hat sie eine fremde Kopfzeile oder fehlt der
 *          gesuchte Abschnitt, greift der mitgelieferte Standard aus
 *          mail_vorlagen_standard.h. Es gibt also keinen Zustand "keine Mail,
 *          weil keine Vorlage".
 */

#ifndef MAIL_VORLAGEN_H
#define MAIL_VORLAGEN_H

#include <Arduino.h>

#include "utils/mail_scheduler.h"
#include "utils/mail_template.h"

class MailVorlagen {
public:
  static constexpr const char* PFAD = "/config/mailvorlagen.txt";

  /// @brief Betreff einer Mailart, Platzhalter bereits ersetzt
  static size_t betreff(Mail::Kind kind, char* out, size_t outSize);

  /// @brief Rumpf zeilenweise expandieren und ausgeben
  static void sendeRumpf(Mail::Kind kind, MailVorlage::ZeilenSenke aus, void* context);

  /// Rohtext für die Weboberfläche - ohne Platzhalterersetzung.
  using RohSenke = void (*)(const char* zeile, void* context);
  static void sendeRoh(MailVorlage::Abschnitt abschnitt, RohSenke aus, void* context);

  /**
   * @brief Eine Vorlage speichern
   * @details Schreibt die Datei neu und übernimmt dabei die übrigen Abschnitte
   *          unverändert. Atomar über eine Zwischendatei und rename(), wie
   *          saveJsonFile() in utils/json_file_utils.h.
   */
  static bool speichere(Mail::Kind kind, const String& betreffText, const String& rumpfText,
                        String& fehler);

  /// @brief Vorlage auf den mitgelieferten Standard zurücksetzen
  static bool setzeZurueck(Mail::Kind kind, String& fehler);

  /// @brief Gemeinsames CSS zeilenweise ausgeben - für den <style>-Block der
  ///        Mail und für die Weboberfläche
  static void sendeStil(MailVorlage::ZeilenSenke aus, void* context);
  static bool speichereStil(const String& css, String& fehler);
  static bool setzeStilZurueck(String& fehler);

  /// @brief Steht für diese Art eine eigene Vorlage in der Datei?
  static bool istAngepasst(Mail::Kind kind);

  static MailVorlage::Abschnitt abschnittFuer(Mail::Kind kind, bool istBetreff);
};

#endif // MAIL_VORLAGEN_H
