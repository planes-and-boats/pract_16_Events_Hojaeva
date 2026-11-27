#pragma once

#include <Windows.h>
#include <string>

const int MIN_PLAYERS = 1;
const int MAX_PLAYERS = 10;

const wchar_t* EVENT_START = L"Local\\StartRaid";
const wchar_t* EVENT_PAUSE = L"Local\\PauseRaid";
const wchar_t* EVENT_TERMINATE = L"Local\\TerminateRaid";