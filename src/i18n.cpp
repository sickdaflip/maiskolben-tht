// SPDX-License-Identifier: GPL-2.0-or-later
#include "i18n.h"

/*
 * Each language is one flash blob of NUL-separated strings in StrId order.
 * en_GB only stores the strings that differ from en_US.
 */

static const char STR_EN_US[] PROGMEM =
	"English (US)\0"
	"Heating\0"
	"Ready\0"
	"Standby\0"
	"Off\0"
	"Error\0"
	"Set\0"
	"No tip detected\0"
	"Check the tip connector\0"
	"Not heating\0"
	"Check the power supply\0"
	"Temperature dropped\0"
	"Tip loose or shorted?\0"
	"Overtemperature\0"
	"Heater shut down\0"
	"Settings\0"
	"Auto standby\0"
	"Heat on power-up\0"
	"Unit\0"
	"Language\0"
	"Temperature offset\0"
	"Swap red/blue\0"
	"Off\0"
	"Yes\0"
	"No\0"
	"Done\0"
	"Which color is shown?\0"
	"Red\0"
	"Blue\0"
	"Hold power for settings\0";

static const char STR_DE[] PROGMEM =
	"Deutsch\0"
	"Heizt\0"
	"Bereit\0"
	"Standby\0"
	"Aus\0"
	"Fehler\0"
	"Soll\0"
	"Keine Spitze erkannt\0"
	"Steckverbindung prüfen\0"
	"Heizt nicht\0"
	"Netzteil prüfen\0"
	"Temperatursturz\0"
	"Spitze locker oder Kurzschluss?\0"
	"Übertemperatur\0"
	"Heizung abgeschaltet\0"
	"Einstellungen\0"
	"Auto-Standby\0"
	"Beim Start heizen\0"
	"Einheit\0"
	"Sprache\0"
	"Temperatur-Offset\0"
	"Rot/Blau tauschen\0"
	"Aus\0"
	"Ja\0"
	"Nein\0"
	"Fertig\0"
	"Welche Farbe siehst du?\0"
	"Rot\0"
	"Blau\0"
	"Power halten: Einstellungen\0";

static const char STR_ES[] PROGMEM =
	"Español\0"
	"Calentando\0"
	"Listo\0"
	"En espera\0"
	"Apagado\0"
	"Error\0"
	"Obj.\0"
	"Punta no detectada\0"
	"Revisa el conector\0"
	"No calienta\0"
	"Revisa la fuente\0"
	"Caída de temperatura\0"
	"¿Punta suelta o en corto?\0"
	"Sobretemperatura\0"
	"Calefactor apagado\0"
	"Ajustes\0"
	"Espera automática\0"
	"Calentar al iniciar\0"
	"Unidad\0"
	"Idioma\0"
	"Ajuste de temperatura\0"
	"Invertir rojo/azul\0"
	"Desact.\0"
	"Sí\0"
	"No\0"
	"Listo\0"
	"¿Qué color ves?\0"
	"Rojo\0"
	"Azul\0"
	"Mantén encendido: ajustes\0";

static const char STR_FR[] PROGMEM =
	"Français\0"
	"Chauffe\0"
	"Prêt\0"
	"Veille\0"
	"Arrêt\0"
	"Erreur\0"
	"Cons.\0"
	"Aucune panne détectée\0"
	"Vérifiez le connecteur\0"
	"Ne chauffe pas\0"
	"Vérifiez l'alimentation\0"
	"Chute de température\0"
	"Panne mal fixée ?\0"
	"Surchauffe\0"
	"Chauffe coupée\0"
	"Réglages\0"
	"Veille auto\0"
	"Chauffer au démarrage\0"
	"Unité\0"
	"Langue\0"
	"Correction temp.\0"
	"Inverser rouge/bleu\0"
	"Désact.\0"
	"Oui\0"
	"Non\0"
	"OK\0"
	"Quelle couleur voyez-vous ?\0"
	"Rouge\0"
	"Bleu\0"
	"Maintenir marche : réglages\0";

static const char GB_LANG_NAME[] PROGMEM = "English (UK)";
static const char GB_COLOR_QUESTION[] PROGMEM = "Which colour is shown?";

static const char *const BLOBS[LANG_COUNT] PROGMEM = {
	STR_EN_US, STR_EN_US, STR_DE, STR_ES, STR_FR
};

static uint8_t current = LANG_EN_US;

void i18nSetLanguage(uint8_t lang) {
	current = lang < LANG_COUNT ? lang : (uint8_t)LANG_EN_US;
}

const char *trLang(uint8_t lang, StrId id) {
	if (lang >= LANG_COUNT) lang = LANG_EN_US;
	if (lang == LANG_EN_GB) {
		if (id == S_LANG_NAME) return GB_LANG_NAME;
		if (id == S_COLOR_QUESTION) return GB_COLOR_QUESTION;
	}
	const char *p = (const char *)pgm_read_ptr(&BLOBS[lang]);
	for (uint8_t i = 0; i < id; i++) {
		while (pgm_read_byte(p)) p++;
		p++;
	}
	return p;
}

const char *tr(StrId id) {
	return trLang(current, id);
}
