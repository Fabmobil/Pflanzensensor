#include <Preferences.h>
#include "preferences_manager.h"
#include "logger/logger.h"

PreferencesManager::PreferencesManager() {}

void PreferencesManager::initNamespace(const String& namespaceName) {
    Preferences prefs;
    if (!prefs.begin(namespaceName.c_str(), false)) {
        Logger::error(F("PreferencesManager"), F("Failed to initialize namespace: ") + namespaceName);
    }
    prefs.end();
}

String PreferencesManager::getString(const String& namespaceName, const String& key, const String& defaultValue) {
    Preferences prefs;
    if (prefs.begin(namespaceName.c_str(), true)) {
        String value = prefs.getString(key.c_str(), defaultValue);
        prefs.end();
        return value;
    } else {
        Logger::error(F("PreferencesManager"), F("Failed to read from namespace: ") + namespaceName);
        return defaultValue;
    }
}

void PreferencesManager::setString(const String& namespaceName, const String& key, const String& value) {
    Preferences prefs;
    if (prefs.begin(namespaceName.c_str(), false)) {
        prefs.putString(key.c_str(), value);
        prefs.end();
    } else {
        Logger::error(F("PreferencesManager"), F("Failed to write to namespace: ") + namespaceName);
    }
}