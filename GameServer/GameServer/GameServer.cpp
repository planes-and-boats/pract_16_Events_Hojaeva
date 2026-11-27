#include <iostream>
#include <exception>
#include <algorithm>
#include <string>
#include "GameDataServer.h"
using namespace std;

HANDLE hStartEvent = NULL;
HANDLE hPauseEvent = NULL;
HANDLE hTerminateEvent = NULL;
HANDLE hBossMassAttack = NULL;
CRITICAL_SECTION cs;

void MixHeroes(const char* pool[], int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        const char* temp = pool[i];
        pool[i] = pool[j];
        pool[j] = temp;
    }
}

string GetTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    return "[" + to_string(st.wHour) + ":" + to_string(st.wMinute) + ":" + to_string(st.wSecond) + "]";
}

bool LitleSleep(int seconds, Raid& raid) {
    int time = 0;
    while (time < seconds * 1000) {
        WaitForSingleObject(hPauseEvent, INFINITE);

        if (raid.isRaidOver || WaitForSingleObject(hTerminateEvent, 0) == WAIT_OBJECT_0) {
            return false;
        }

        Sleep(TICK_MS);
        time += TICK_MS;
    }
    return true;
}

void PlayerAttackBoss(Player& player, Raid& raid, int baseDamage, bool isSpecial) {
    if (raid.isRaidOver || player.health <= 0) return;

    long long damage = baseDamage * (DEFENSE_MAX - raid.boss.resist) / DEFENSE_MAX;
    raid.boss.health -= damage;
    player.totalDamage += damage;

    if (raid.boss.health < 0) raid.boss.health = 0;

    if (isSpecial) {
        cout << GetTime() << " [УМЕНИЕ] " << player.name << " применяет спецприем и наносит Повелителю Тьмы "
            << damage << " урона (HP босса: " << raid.boss.health << ")" << endl;
    }
    else {
        cout << GetTime() << " [АТАКА] " << player.name << " атакует Повелителя Тьмы на "
            << damage << " урона (HP босса: " << raid.boss.health << ")" << endl;
    }

    if (raid.boss.health <= 0) {
        raid.boss.health = 0;
        raid.isRaidOver = true;
        cout << endl << GetTime() << " [ПОБЕДА] Повелитель Тьмы повержен! Решающий удар нанес " << player.name << "!" << endl;
    }
}

void PlayerTakeDamage(Player& player, Raid& raid, int bossDamage, bool isMassAttack) {
    if (player.health <= 0 || raid.isRaidOver) return;

    if (rand() % DODGE_MAX < player.dodgeChance) {
        cout << GetTime() << " [УКЛОНЕНИЕ] " << player.name << " уклонился от атаки Повелителя Тьмы!" << endl;
        return;
    }

    int effectiveBossDamage = bossDamage;
    if (isMassAttack && raid.alivePlayers > 1) {
        effectiveBossDamage = static_cast<int>(bossDamage * (1.0 - 0.05 * (raid.alivePlayers - 1)));
    }

    long long damage = effectiveBossDamage * (DEFENSE_MAX - player.defense) / DEFENSE_MAX;
    player.health -= damage;
    if (player.health < 0) player.health = 0;

    cout << GetTime() << " [УРОН] " << player.name << " получает " << damage
        << " урона (осталось " << player.health << " HP)" << endl;

    if (player.health <= 0) {
        player.health = 0;
        raid.alivePlayers--;
        cout << GetTime() << " [ГИБЕЛЬ] " << player.name << " пал в бою! В строю осталось: " << raid.alivePlayers << endl;

        if (raid.alivePlayers == 0) {
            raid.isRaidOver = true;
            cout << endl << GetTime() << " [ПОРАЖЕНИЕ] Последний герой гильдии пал в бою. Тьма восторжествовала." << endl;
        }
    }
}

DWORD WINAPI BossThread(LPVOID lpParam) {
    Raid* raid = (Raid*)lpParam;
    int ordinaryTimer = 0;
    int specialTimer = 0;

    while (!raid->isRaidOver) {
        if (!LitleSleep(1, *raid)) break;

        ordinaryTimer++;
        specialTimer++;

        if (ordinaryTimer == raid->boss.attackCooldown) {
            ordinaryTimer = 0;
            EnterCriticalSection(&cs);

            if (raid->alivePlayers > 0 && !raid->isRaidOver) {
                int randIndex = rand() % raid->totalPlayers;
                while (raid->players[randIndex].health <= 0) {
                    randIndex = rand() % raid->totalPlayers;
                }
                cout << GetTime() << " [БОСС] Повелитель Тьмы обрушивает удар на " << raid->players[randIndex].name << "!" << endl;
                SetEvent(raid->players[randIndex].hSingleAttackEvent);
            }

            LeaveCriticalSection(&cs);
        }

        if (specialTimer == raid->boss.specialCooldown) {
            specialTimer = 0;
            EnterCriticalSection(&cs);

            if (raid->alivePlayers > 0 && !raid->isRaidOver) {
                cout << GetTime() << " [БОСС | МАССОВАЯ АТАКА] Повелитель Тьмы высвобождает волну темной энергии по всему отряду!" << endl;
                PulseEvent(hBossMassAttack);
            }

            LeaveCriticalSection(&cs);
        }
    }
    return 0;
}

DWORD WINAPI PlayerThread(LPVOID lpParam) {
    PlayerParam* param = (PlayerParam*)lpParam;
    Raid* raid = param->raid;
    Player& current = raid->players[param->index];

    int attackTimer = 0;
    int specialAttackTimer = 0;

    while (!raid->isRaidOver && current.health > 0) {
        WaitForSingleObject(hPauseEvent, INFINITE);

        HANDLE eventsToWait[3] = { hTerminateEvent, current.hSingleAttackEvent, hBossMassAttack };
        DWORD waitResult = WaitForMultipleObjects(3, eventsToWait, FALSE, TICK_MS);

        if (waitResult == WAIT_OBJECT_0) break;
        else if (waitResult == WAIT_OBJECT_0 + 1 || waitResult == WAIT_OBJECT_0 + 2) {
            EnterCriticalSection(&cs);

            if (current.health > 0) {
                bool isMass = (waitResult == WAIT_OBJECT_0 + 2);
                int bossDamage = isMass ? raid->boss.specialDamage : raid->boss.damage;
                PlayerTakeDamage(current, *raid, bossDamage, isMass);
            }

            LeaveCriticalSection(&cs);
        }
        else if (waitResult == WAIT_TIMEOUT) {
            attackTimer += TICK_MS;
            specialAttackTimer += TICK_MS;

            if (attackTimer >= current.attakCooldown * 1000) {
                attackTimer = 0;
                EnterCriticalSection(&cs);
                PlayerAttackBoss(current, *raid, current.damage, false);
                LeaveCriticalSection(&cs);
            }

            if (specialAttackTimer >= current.specialCooldown * 1000) {
                specialAttackTimer = 0;
                EnterCriticalSection(&cs);
                PlayerAttackBoss(current, *raid, current.specialDamage, true);
                LeaveCriticalSection(&cs);
            }
        }
    }
    return 0;
}

void InitializeWork() {
    hStartEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_START);
    if (hStartEvent == NULL) throw runtime_error("не удалось открыть событие старта.");

    hPauseEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_PAUSE);
    if (hPauseEvent == NULL) throw runtime_error("не удалось открыть событие паузы.");

    hTerminateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_TERMINATE);
    if (hTerminateEvent == NULL) throw runtime_error("не удалось открыть событие завершения.");

    hBossMassAttack = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hBossMassAttack == NULL) throw runtime_error("не удалось создать событие массовой атаки босса.");

    InitializeCriticalSection(&cs);
    MixHeroes(GUILD_HEROES, HEROES_SIZE);
}

void InitializePlayers(Raid& raid, int count) {
    raid.totalPlayers = count;
    raid.alivePlayers = count;
    raid.players = new Player[count];

    cout << "СОСТАВ УДАРНОГО ОТРЯДА:" << endl;
    for (int i = 0; i < count; i++) {
        sprintf_s(raid.players[i].name, 64, "%s", GUILD_HEROES[i]);
        cout << " - " << raid.players[i].name << " (HP: " << raid.players[i].health << ")" << endl;

        raid.players[i].hSingleAttackEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (raid.players[i].hSingleAttackEvent == NULL) {
            throw runtime_error("не удалось создать событие единоличной атаки для " + string(raid.players[i].name) + '.');
        }
    }
    cout << endl;
}

void CompletionWork(Raid& raid) {
    CloseHandle(hStartEvent);
    CloseHandle(hPauseEvent);
    CloseHandle(hTerminateEvent);
    CloseHandle(hBossMassAttack);

    for (int i = 0; i < raid.totalPlayers; i++) {
        CloseHandle(raid.players[i].hSingleAttackEvent);
    }
    delete[] raid.players;
    DeleteCriticalSection(&cs);
}

void ShowRecords(Raid& raid, double durationSec) {
    cout << endl << "[ИТОГИ СРАЖЕНИЯ]" << endl << endl;

    if (raid.boss.health <= 0) {
        cout << "Победитель: Отряд гильдии (Повелитель Тьмы повержен)" << endl;
    }
    else if (raid.alivePlayers <= 0) {
        cout << "Победитель: Повелитель Тьмы" << endl;
    }
    else {
        cout << "Исход: Отряд эвакуирован по приказу Лидера" << endl;
    }

    cout << "Продолжительность боя: " << (int)durationSec << " сек." << endl << endl;

    sort(raid.players, raid.players + raid.totalPlayers, [](const Player& a, const Player& b) {
        return a.totalDamage > b.totalDamage;
        });

    int min = (raid.totalPlayers >= 3) ? 3 : raid.totalPlayers;

    cout << "ТОП ГЕРОЕВ ПО НАНЕСЕННОМУ УРОНУ:" << endl;
    cout << "Место\tГерой\t\t\tУрон" << endl;
    for (int i = 0; i < min; i++) {
        cout << i + 1 << ".\t" << raid.players[i].name << "\t\t\t" << raid.players[i].totalDamage << endl;
    }
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "Russian");
    srand(GetTickCount());

    try {
        DWORD dungeonId = GetCurrentProcessId();

        cout << endl << "[ПОДЗЕМЕЛЬЕ #" << dungeonId << " | ТРОННЫЙ ЗАЛ ТЬМЫ]" << endl << endl;

        int count;
        if (argc < 2) throw invalid_argument("ожидалось получить количество игроков.");
        try { count = stoi(argv[1]); }
        catch (exception&) { throw invalid_argument("ожидалось числовое значение."); }
        if (count < 1 || count > 10) throw invalid_argument("ожидалось числовое значение от 1 до 10.");

        InitializeWork();

        Raid raid;
        cout << "ПРОТИВНИК: Повелитель Тьмы (HP: " << raid.boss.health << ", Сопротивление: " << raid.boss.resist << "%)" << endl << endl;

        InitializePlayers(raid, count);

        cout << GetTime() << " Отряд занял позиции в Подземелье #" << dungeonId << ". Ожидание приказа Лидера из штаба..." << endl << endl;

        HANDLE eventsToWait[2] = {hStartEvent, hTerminateEvent};
        DWORD waitResult = WaitForMultipleObjects(2, eventsToWait, FALSE, INFINITE);

        switch (waitResult) {
        case WAIT_OBJECT_0: {
            cout << GetTime() << " Сигнал получен! Штурм Повелителя Тьмы начался!" << endl << endl;

            long long activeCombatMs = 0;

            HANDLE hBossThread = CreateThread(NULL, 0, BossThread, &raid, 0, NULL);
            if (hBossThread == NULL) throw runtime_error("не удалось создать поток босса.");

            HANDLE* hPlayerThreads = new HANDLE[count];
            PlayerParam* param = new PlayerParam[count];

            for (int i = 0; i < count; i++) {
                param[i] = { &raid, i };
                hPlayerThreads[i] = CreateThread(NULL, 0, PlayerThread, &param[i], 0, NULL);
                if (hPlayerThreads[i] == NULL) throw runtime_error("не удалось создать поток " + string(raid.players[i].name) + '.');
            }

            while (!raid.isRaidOver && WaitForSingleObject(hTerminateEvent, 0) != WAIT_OBJECT_0) {
                WaitForSingleObject(hPauseEvent, INFINITE);
                Sleep(TICK_MS);
                activeCombatMs += TICK_MS;
            }

            WaitForSingleObject(hBossThread, INFINITE);
            WaitForMultipleObjects(count, hPlayerThreads, TRUE, INFINITE);

            CloseHandle(hBossThread);
            for (int i = 0; i < count; i++) CloseHandle(hPlayerThreads[i]);
            delete[] hPlayerThreads;
            delete[] param;

            ShowRecords(raid, activeCombatMs / 1000.0);
            break;
        }
        case WAIT_OBJECT_0 + 1: {
            cout << GetTime() << " Экстренная эвакуация до начала сражения." << endl;
            break;
        }
        case WAIT_FAILED: {
            throw runtime_error("ошибка ожидания событий.");
        }
        }

        CompletionWork(raid);

        cout << endl << "[ЗАВЕРШЕНИЕ РЕЙДА | ПОДЗЕМЕЛЬЕ #" << dungeonId << "]" << endl << endl;
        cout << "Портал в подземелье закрыт, отряд вернулся в штаб гильдии." << endl;
        cout << "Летопись сражения передана Лидеру." << endl;
    }
    catch (exception& e) {
        cout << endl << "Ошибка: " << e.what() << endl;
    }

    cout << endl << "Нажмите любую клавишу, чтобы покинуть подземелье...";
    system("pause > nul");
    return 0;
}
