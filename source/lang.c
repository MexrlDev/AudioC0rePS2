#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "lang.h"
#include "log.h"

static const char *g_tab[][L_COUNT] = {
/* EN */ {
    "NOW PLAYING","tracks","Source: %s",
    "PLAYING","PAUSED","MUTED","UNMUTED",
    "LOOP ON","LOOP OFF","AUTO SKIP ON","AUTO SKIP OFF",
    "SEEK +","SEEK -","VOLUME",
    "NEXT TRACK","PREV TRACK",
    "NO NEXT SONG","NO PREV SONG","END OF SONG","START OF SONG",
    "Loading","LOAD FAILED",
    "LOG ON","LOG OFF","NO COVER",
    "NO MUSIC FOUND","NO MUSIC SOURCE","FOLDER IS EMPTY",
    "SAVE THIS SOURCE?","YES","NO","CANCEL",
    "X: Play  O: Back  Sq: Refresh  Tri: Settings  Sel: Log  Start: Quit",
    "O: Back  X: Play/Pause  L1/R1: Track  L/R: Seek  U/D: Vol",
    "Sq: Mute  Tri: Loop  Sel: Auto-Skip",
    "SETTINGS","Music Source","Language","Reset to Defaults",
    "Save & Exit","Cancel",
    "X: Select   O: Back   Start: Save & Exit",
    "O: Back",
    "Settings reset","Saved","Save failed","Reset all settings?",
    "Device disconnected","Press X to rescan"
},
/* RU */ {
    "СЕЙЧАС ИГРАЕТ","треков","Источник: %s",
    "ИГРАЕТ","ПАУЗА","БЕЗ ЗВУКА","ЗВУК ВКЛ",
    "ПОВТОР ВКЛ","ПОВТОР ВЫКЛ","АВТО ВКЛ","АВТО ВЫКЛ",
    "ВПЕРЁД","НАЗАД","ГРОМКОСТЬ",
    "СЛЕД ТРЕК","ПРЕД ТРЕК",
    "НЕТ СЛЕД","НЕТ ПРЕД","КОНЕЦ ПЕСНИ","НАЧАЛО",
    "Загрузка","ОШИБКА ЗАГРУЗКИ",
    "ЛОГ ВКЛ","ЛОГ ВЫКЛ","НЕТ ОБЛОЖКИ",
    "МУЗЫКА НЕ НАЙДЕНА","НЕТ ИСТОЧНИКА","ПАПКА ПУСТА",
    "СОХРАНИТЬ?","ДА","НЕТ","ОТМЕНА",
    "X: Играть  O: Назад  Sq: Обновить  Tri: Настройки  Sel: Лог  Start: Выход",
    "O: Назад  X: Пауза  L1/R1: Трек  L/R: Поиск  U/D: Громкость",
    "Sq: Звук  Tri: Повтор  Sel: Авто",
    "НАСТРОЙКИ","Источник музыки","Язык","Сбросить настройки",
    "Сохранить и выйти","Отмена",
    "X: Выбрать   O: Назад   Start: Сохранить",
    "O: Назад",
    "Настройки сброшены","Сохранено","Ошибка сохранения","Сбросить все?",
    "Устройство отключено","Нажмите X"
},
/* PT-BR */ {
    "TOCANDO AGORA","faixas","Fonte: %s",
    "TOCANDO","PAUSADO","MUDO","SOM LIGADO",
    "REPETIR ON","REPETIR OFF","AUTO ON","AUTO OFF",
    "AVANCAR","VOLTAR","VOLUME",
    "PROXIMA","ANTERIOR",
    "SEM PROXIMA","SEM ANTERIOR","FIM DA MUSICA","INICIO",
    "Carregando","FALHA AO CARREGAR",
    "LOG ON","LOG OFF","SEM CAPA",
    "SEM MUSICA","SEM FONTE","PASTA VAZIA",
    "SALVAR?","SIM","NAO","CANCELAR",
    "X: Tocar  O: Voltar  Sq: Atualizar  Tri: Ajustes  Sel: Log  Start: Sair",
    "O: Voltar  X: Play/Pause  L1/R1: Faixa  L/R: Avancar  U/D: Volume",
    "Sq: Mudo  Tri: Repetir  Sel: Auto-Skip",
    "AJUSTES","Fonte de musica","Idioma","Restaurar padroes",
    "Salvar e sair","Cancelar",
    "X: Selecionar   O: Voltar   Start: Salvar",
    "O: Voltar",
    "Ajustes restaurados","Salvo","Falha ao salvar","Restaurar todos?",
    "Dispositivo desconectado","Pressione X"
},
/* FR */ {
    "EN LECTURE","pistes","Source : %s",
    "LECTURE","PAUSE","MUET","SON ACTIF",
    "REPETER ON","REPETER OFF","AUTO ON","AUTO OFF",
    "AVANCER","RECULER","VOLUME",
    "SUIVANT","PRECEDENT",
    "PAS DE SUIVANT","PAS DE PRECEDENT","FIN DU MORCEAU","DEBUT",
    "Chargement","ECHEC DU CHARGEMENT",
    "LOG ON","LOG OFF","PAS DE COUVERTURE",
    "AUCUNE MUSIQUE","AUCUNE SOURCE","DOSSIER VIDE",
    "SAUVEGARDER?","OUI","NON","ANNULER",
    "X: Lire  O: Retour  Sq: Rafraichir  Tri: Reglages  Sel: Log  Start: Quitter",
    "O: Retour  X: Lecture  L1/R1: Piste  L/R: Avancer  U/D: Volume",
    "Sq: Muet  Tri: Repeter  Sel: Auto-Skip",
    "REGLAGES","Source musique","Langue","Reinitialiser",
    "Enregistrer et quitter","Annuler",
    "X: Choisir   O: Retour   Start: Enregistrer",
    "O: Retour",
    "Reglages reinitialises","Enregistre","Echec","Reinitialiser tout?",
    "Appareil deconnecte","Appuyez sur X"
},
/* DE */ {
    "SPIELT GERADE","Titel","Quelle: %s",
    "SPIELT","PAUSE","STUMM","TON AN",
    "WIEDERH AN","WIEDERH AUS","AUTO AN","AUTO AUS",
    "VORSPULEN","ZURUCKSPULEN","LAUTSTAERKE",
    "NAECHSTER","VORHERIGER",
    "KEIN NAECHSTER","KEIN VORHERIGER","ENDE DES LIEDS","ANFANG",
    "Lade","LADEN FEHLGESCHLAGEN",
    "LOG AN","LOG AUS","KEIN COVER",
    "KEINE MUSIK","KEINE QUELLE","ORDNER LEER",
    "SPEICHERN?","JA","NEIN","ABBRECHEN",
    "X: Spielen  O: Zuruck  Sq: Aktual  Tri: Einstellungen  Sel: Log  Start: Beenden",
    "O: Zuruck  X: Play/Pause  L1/R1: Titel  L/R: Spulen  U/D: Lautstaerke",
    "Sq: Stumm  Tri: Wiederholen  Sel: Auto-Skip",
    "EINSTELLUNGEN","Musikquelle","Sprache","Zurucksetzen",
    "Speichern und beenden","Abbrechen",
    "X: Wahlen   O: Zuruck   Start: Speichern",
    "O: Zuruck",
    "Einstellungen zuruckgesetzt","Gespeichert","Fehler","Alle zurucksetzen?",
    "Gerat getrennt","X drucken"
},
/* TR */ {
    "SIMDI CALIYOR","parca","Kaynak: %s",
    "CALIYOR","DURAKLATILDI","SESSIZ","SES ACIK",
    "TEKRAR AC","TEKRAR KAPAT","OTO AC","OTO KAPAT",
    "ILERI","GERI","SES SEVIYESI",
    "SONRAKI PARCA","ONCEKI PARCA",
    "SONRAKI YOK","ONCEKI YOK","PARCANIN SONU","BASLANGIC",
    "Yukleniyor","YUKLEME BASARISIZ",
    "LOG AC","LOG KAPAT","KAPAK YOK",
    "MUZIK YOK","KAYNAK YOK","KLASOR BOS",
    "KAYDET?","EVET","HAYIR","IPTAL",
    "X: Cal  O: Geri  Sq: Yenile  Tri: Ayarlar  Sel: Log  Start: Cikis",
    "O: Geri  X: Duraklat  L1/R1: Parca  L/R: Ileri  U/D: Ses",
    "Sq: Sessiz  Tri: Tekrar  Sel: Oto-Atla",
    "AYARLAR","Muzik kaynagi","Dil","Varsayilana dondur",
    "Kaydet ve cik","Iptal",
    "X: Sec   O: Geri   Start: Kaydet",
    "O: Geri",
    "Ayarlar sifirlandi","Kaydedildi","Basarisiz","Tumu sifirlansin?",
    "Cihaz baglantisi kesildi","X'e basin"
}
};

static const char *k_names[] = {
    "English", "Русский",
    "Portugues (BR)", "Francais", "Deutsch", "Turkce"
};

static int g_lang = 0;

void lang_init(void) { g_lang = 0; }

const char *lang_str(lang_id_t id)
{
    if (id < 0 || id >= L_COUNT) return "";
    return g_tab[g_lang][id];
}

int         lang_current(void) { return g_lang; }
int         lang_count(void)   { return (int)(sizeof g_tab / sizeof g_tab[0]); }
const char *lang_name(int idx) {
    if (idx < 0 || idx >= lang_count()) return "";
    return k_names[idx];
}
void        lang_set(int idx)  { if (idx >= 0 && idx < lang_count()) g_lang = idx; }