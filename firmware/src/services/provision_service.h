/**
 * @file provision_service.h
 * @brief Hardware und Einstellungen aus dem Webflasher übernehmen
 * @details Zwei Schritte beim Start:
 *          1. loadHardware() - vor ConfigService::load(): Hardware aus dem
 *             Provisionierungssektor setzen (sonst Profil-Voreinstellung).
 *          2. applySettings() - nach ConfigService::load(): eine mitgegebene
 *             Konfigurationsdatei importieren, dann Name, WLAN 1-3, Kanalnamen und
 *             Adminpasswort übernehmen und den Sektor ohne sie zurückschreiben.
 */
#pragma once

#include "core/hardware_check.h"
#include "core/provision.h"

namespace ps {

class ConfigService;

class ProvisionService {
public:
  ProvisionService() = default;
  ~ProvisionService() { releaseBlock(); }
  ProvisionService(const ProvisionService&) = delete;
  ProvisionService& operator=(const ProvisionService&) = delete;

  void loadHardware();
  /// true, wenn Einstellungen übernommen wurden
  bool applySettings(ConfigService& cfg);

  provision::Result readResult() const { return read_; }
  /// Hardware stammt aus dem Webflasher (sonst Voreinstellung des Profils)
  bool fromWebflasher() const { return fromBlock_; }
  HwCheckResult hardwareProblem() const { return problem_; }

private:
  static void applyChannelName(ConfigService& cfg, const provision::ChannelName& cn);
  /// "settings.config" (Konfigurationsdatei aus dem Webflasher) importieren
  static void importConfig(ConfigService& cfg);
  /// Block freigeben, Passwörter vorher löschen (auch ohne applySettings())
  void releaseBlock();
  provision::Result read_ = provision::Result::Empty;
  bool fromBlock_ = false;
  HwCheckResult problem_;
  /// Nur bis applySettings(): danach gelöscht
  provision::Block* block_ = nullptr;
};

} // namespace ps
