#include <iostream>
#include <string>
#include <exception>
#include <conio.h>
#include <Windows.h>
#include "RaidDataManagement.h"

using namespace std;

int main()
{
    setlocale(LC_ALL, "Russian");

    try {
        DWORD guildId = GetCurrentProcessId();

        cout << endl << "[СИСТЕМА ШТАБА | ГИЛЬДИЯ #" << guildId << "]" << endl << endl;

        cout << "Добро пожаловать, Глава гильдии #" << guildId << "." << endl;
        cout << "Тактический терминал готов к работе. Ваши бойцы ждут распоряжений." << endl << endl;

        cout << "ПЛАН И ПРАВИЛА РЕЙДА:" << endl;
        cout << "1. Главная цель — Повелитель Тьмы." << endl;
        cout << "2. В рейд допускается от " << MIN_PLAYERS << " до " << MAX_PLAYERS << " членов гильдии." << endl;
        cout << "3. Отряд переносится в подземелье и ждет вашего сигнала к атаке." << endl;
        cout << "4. Сражение идет до полного падения босса или гибели всего отряда." << endl << endl;

        HANDLE hStartEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_START);
        if (hStartEvent == NULL) {
            throw runtime_error("не удалось инициализировать канал старта.");
        }

        HANDLE hPauseEvent = CreateEvent(NULL, TRUE, TRUE, EVENT_PAUSE);
        if (hPauseEvent == NULL) {
            throw runtime_error("не удалось инициализировать канал паузы.");
        }

        HANDLE hTerminateEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_TERMINATE);
        if (hTerminateEvent == NULL) {
            throw runtime_error("не удалось инициализировать канал эвакуации.");
        }

        bool isWorking = true;
        bool isStarted = false;
        bool isPaused = false;
        bool isTerminated = false;

        string input;
        int choice;
        int count;

        cout << "Сколько бойцов вы отправляете на штурм, Лидер (от " << MIN_PLAYERS << " до " << MAX_PLAYERS << ")? > ";
        cin >> input;

        try {
            count = stoi(input);
        }
        catch (exception&) {
            throw invalid_argument("требуется ввести число.");
        }

        if (count < MIN_PLAYERS || count > MAX_PLAYERS) {
            throw invalid_argument("численность отряда должна быть от 1 до 10.");
        }

        wstring cmdLine = L"C:\\Users\\st312-11\\Documents\\hojaeva pr-31\\16\\GameServer\\x64\\Debug\\GameServer.exe " + to_wstring(count);
        /*wstring cmdLine = L"C:\\Users\\Пользователь\\Desktop\\Бред\\СП\\16\\GameServer\\x64\\Debug\\GameServer.exe "
            + to_wstring(count);*/

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(STARTUPINFO);

        if (!CreateProcess(0, (LPWSTR)cmdLine.c_str(), NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            throw runtime_error("не удалось открыть портал в подземелье.");
        }

        DWORD dungeonId = pi.dwProcessId;

        CloseHandle(pi.hThread);

        cout << endl << "Портал открыт, Глава! Отряд (" << count << " чел.) успешно перенесен в Подземелье #" << dungeonId << "." << endl;
        cout << "Связь с группой установлена. Отдавайте приказы, когда будете готовы." << endl;

        while (isWorking) {
            cout << endl << "КОМАНДНЫЙ ПУЛЬТ | Подземелье #" << dungeonId << endl;
            cout << "1. Начать штурм" << endl;
            cout << "2. Тактическая пауза" << endl;
            cout << "3. Продолжить атаку" << endl;
            cout << "4. Экстренная эвакуация" << endl;
            cout << "0. Покинуть штаб" << endl << endl;

            try {
                cout << "Ваш приказ, Лидер > ";
                cin >> input;

                try {
                    choice = stoi(input);
                }
                catch (exception&) {
                    throw invalid_argument("неверный формат команды.");
                }

                if (choice >= 1 && choice <= 4 && WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
                    throw logic_error("связь с Подземельем #" + to_string(dungeonId) + " потеряна: рейд уже завершен.");
                }

                switch (choice) {
                case 1:
                    if (isTerminated) {
                        throw logic_error("невозможно начать штурм после эвакуации.");
                    }
                    if (isStarted) {
                        throw logic_error("штурм Повелителя Тьмы уже идет.");
                    }
                    isStarted = true;
                    cout << "[Штаб -> Подземелье #" << dungeonId << "] Приказ принят: отряд атакует Повелителя Тьмы!" << endl;
                    SetEvent(hStartEvent);
                    break;

                case 2:
                    if (isTerminated) {
                        throw logic_error("рейд уже завершен эвакуацией.");
                    }
                    if (!isStarted) {
                        throw logic_error("нельзя поставить на паузу бой, который еще не начался.");
                    }
                    if (isPaused) {
                        throw logic_error("бой уже находится на тактической паузе.");
                    }
                    isPaused = true;
                    cout << "[Штаб -> Подземелье #" << dungeonId << "] Приказ принят: бой приостановлен, тактическая пауза." << endl;
                    ResetEvent(hPauseEvent);
                    break;

                case 3:
                    if (isTerminated) {
                        throw logic_error("рейд уже завершен эвакуацией.");
                    }
                    if (!isStarted) {
                        throw logic_error("штурм еще не начался.");
                    }
                    if (!isPaused) {
                        throw logic_error("бой не находится на паузе.");
                    }
                    isPaused = false;
                    cout << "[Штаб -> Подземелье #" << dungeonId << "] Приказ принят: пауза снята, штурм продолжается!" << endl;
                    SetEvent(hPauseEvent);
                    break;

                case 4:
                    if (isTerminated) {
                        throw logic_error("экстренная эвакуация уже была объявлена.");
                    }
                    isTerminated = true;
                    isPaused = false;
                    cout << "[Штаб -> Подземелье #" << dungeonId << "] Приказ принят: активация свитков экстренного отступления!" << endl;
                    SetEvent(hPauseEvent);
                    SetEvent(hTerminateEvent);
                    break;

                case 0:
                    isWorking = false;
                    break;

                default:
                    throw invalid_argument("неизвестный код приказа.");
                }
            }
            catch (exception& e) {
                cout << endl << "Ошибка: " << e.what() << endl;
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(hStartEvent);
        CloseHandle(hPauseEvent);
        CloseHandle(hTerminateEvent);

        cout << endl << "[ЗАВЕРШЕНИЕ РАБОТЫ ШТАБА | ГИЛЬДИЯ #" << guildId << "]" << endl << endl;
        cout << "Все каналы связи с подземельем закрыты, системные ресурсы освобождены." << endl;
        cout << "Благодарим за отличное командование, Глава гильдии #" << guildId << "! До новых рейдов." << endl;
    }
    catch (exception& e) {
        cout << endl << "Критическая ошибка: " << e.what() << endl;
    }

    cout << endl << "Нажмите любую клавишу для выхода из терминала...";
    _getch();
    return 0;
}
