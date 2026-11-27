#pragma once
#include <Windows.h>

const int TICK_MS = 100;
const int DODGE_MAX = 100;
const int DEFENSE_MAX = 100;

const wchar_t* EVENT_START = L"Local\\StartRaid";
const wchar_t* EVENT_PAUSE = L"Local\\PauseRaid";
const wchar_t* EVENT_TERMINATE = L"Local\\TerminateRaid";

struct Bayum {
    long long health = 2200000;
    int resist = 44;
    int damage = 73843;
    int specialDamage = 150000;
    int attackCooldown = 5;
    int specialCooldown = 10;
};

struct Player {
    long long health = 500000;
    int damage = 12000;
    int specialDamage = 30000;
    int attakCooldown = 2;
    int specialCooldown = 5;
    int defense = 20;
    int dodgeChance = 15;
    char name[64];
    long long totalDamage = 0;
    HANDLE hSingleAttackEvent = NULL;
};

struct Raid {
    Bayum boss;
    Player* players = nullptr;
    int totalPlayers = 0;
    int alivePlayers = 0;
    bool isRaidOver = false;
};

struct PlayerParam {
    Raid* raid;
    int index;
};

const int HEROES_SIZE = 18;
const char* GUILD_HEROES[HEROES_SIZE] = {
    "[Берсерк] Гатс",
    "[Ведьмак] Геральт",
    "[Чародейка] Йеннифэр",
    "[Танк] Бастион",
    "[Паладин] Люциан",
    "[Маг] Мерлин",
    "[Ассасин] Каин",
    "[Следопыт] Орион",
    "[Жрец] Серафим",
    "[Чернокнижник] Ксавьер",
    "[Воин] Рагнар",
    "[Стрелок] Феникс",
    "[Рыцарь] Ланселот",
    "[Некромант] Анубис",
    "[Страж] Титан",
    "[Клинок] Акира",
    "[Друид] Эш",
    "[Инквизитор] Соломон"
};