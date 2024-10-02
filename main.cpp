#include <windows.h>
#include <d3d9.h>
#include "toml++/toml.hpp"

#include "nya_dx9_hookbase.h"
#include "nya_commonmath.h"
#include "nya_commonhooklib.h"

#include "fouc.h"

float fMenuYTop = 0.18;
int nMenuYSize = 16;

bool bMenuUp = false;
int nSelectedOption = 2;
int nTempOptionCounter = 6;
int nMenuScroll = 0;
std::string sEnterHint;

LiteDb* pCurrentNode = nullptr;
const char* sCurrentProperty = nullptr;
void* pCurrentPropertyEditing = nullptr;
std::string sCurrentPropertyEditString;

bool IsTypeableCharacterInFO2(wchar_t c) {
	// number row
	if (c == '`') return true;
	if (c == '1') return true;
	if (c == '2') return true;
	if (c == '3') return true;
	if (c == '4') return true;
	if (c == '5') return true;
	if (c == '6') return true;
	if (c == '7') return true;
	if (c == '8') return true;
	if (c == '9') return true;
	if (c == '0') return true;
	if (c == '-') return true;
	if (c == '=') return true;

	// number row + shift
	if (c == '~') return true;
	if (c == '!') return true;
	if (c == '@') return true;
	if (c == '#') return true;
	if (c == '$') return true;
	//if (c == '%') return true; // used for printf
	if (c == '^') return true;
	if (c == '&') return true;
	if (c == '*') return true;
	if (c == '(') return true;
	if (c == ')') return true;
	if (c == '_') return true;
	if (c == '+') return true;

	// letters
	if (c >= 'a' && c <= 'z') return true;
	if (c >= 'A' && c <= 'Z') return true;

	// symbols next to enter
	if (c == '[') return true;
	if (c == ']') return true;
	if (c == ';') return true;
	if (c == '\'') return true;
	if (c == '\\') return true;
	if (c == ',') return true;
	if (c == '.') return true;
	if (c == '/') return true;
	if (c == '{') return true;
	if (c == '}') return true;
	if (c == ':') return true;
	if (c == '"') return true;
	if (c == '|') return true;
	if (c == '<') return true;
	if (c == '>') return true;
	if (c == '?') return true;

	// spacebar
	if (c == ' ') return true;
	return false;
}

bool IsTypeableNumberInFO2(wchar_t c) {
	if (c == '1') return true;
	if (c == '2') return true;
	if (c == '3') return true;
	if (c == '4') return true;
	if (c == '5') return true;
	if (c == '6') return true;
	if (c == '7') return true;
	if (c == '8') return true;
	if (c == '9') return true;
	if (c == '0') return true;
	if (c == '-') return true;
	if (c == '.') return true;
	return false;
}

bool IsStringValidForFO2Drawing(const std::string& str, bool numbersOnly) {
	if (numbersOnly) {
		for (auto& c: str) {
			if (!IsTypeableNumberInFO2(c)) return false;
		}
	}
	else {
		for (auto& c: str) {
			if (!IsTypeableCharacterInFO2(c)) return false;
		}
	}
	return true;
}

std::string GetClipboardText() {
	if (!OpenClipboard(nullptr)) return "";

	HANDLE hData = GetClipboardData(CF_TEXT);
	if (!hData) return "";

	auto pszText = (char*)GlobalLock(hData);
	if (!pszText) return "";

	std::string text(pszText);
	GlobalUnlock(hData);
	CloseClipboard();
	return text;
}

void AddTextInputToString(char* str, int len, bool numbersOnly) {
	std::string text = str;
	if (text.length() < len - 1 && IsStringValidForFO2Drawing(sKeyboardInput, numbersOnly)) text += sKeyboardInput;
	if (IsKeyJustPressed(VK_BACK) && !text.empty()) text.pop_back();
	if (IsKeyPressed(VK_CONTROL) && IsKeyJustPressed('V')) {
		text += GetClipboardText();
	}
	if (text.length() < len && IsStringValidForFO2Drawing(text, numbersOnly)) strcpy_s(str, len, text.c_str());
}

const char* GetPropertyTypeString(int id) {
	switch (id) {
		case DBVALUE_CHAR:
			return "char";
		case DBVALUE_STRING:
			return "string";
		case DBVALUE_BOOL:
			return "bool";
		case DBVALUE_INT:
			return "int";
		case DBVALUE_FLOAT:
			return "float";
		case DBVALUE_RGBA:
			return "RGBA";
		case DBVALUE_VECTOR2:
			return "Vector2";
		case DBVALUE_VECTOR3:
			return "Vector3";
		case DBVALUE_VECTOR4:
			return "Vector4";
		case DBVALUE_NODE:
			return "node";
		default:
			return "UNKNOWN";
	}
}

int GetPropertyTypeSize(int type, bool use4ByteForVectors) {
	switch (type) {
		case DBVALUE_CHAR:
			return 1;
		case DBVALUE_STRING:
			return 1;
		case DBVALUE_BOOL:
			return 4;
		case DBVALUE_INT:
			return 4;
		case DBVALUE_FLOAT:
			return 4;
		case DBVALUE_RGBA:
			return 4;
		case DBVALUE_VECTOR2:
			return use4ByteForVectors ? 4 : 2*4;
		case DBVALUE_VECTOR3:
			return use4ByteForVectors ? 4 : 3*4;
		case DBVALUE_VECTOR4:
			return use4ByteForVectors ? 4 : 4*4;
		case DBVALUE_NODE:
			return 2;
		default:
			return 1;
	}
}

bool DrawMenuOption(const std::string& string, bool grayedOut = false) {
	int scroll = nTempOptionCounter - nMenuScroll;
	if (scroll < 0 || scroll > nMenuYSize) {
		nTempOptionCounter++;
		if (scroll == -1) {
			tNyaStringData data;
			data.x = 0.5;
			data.y = fMenuYTop;
			data.size = 0.03;
			data.y += data.size * scroll;
			data.XCenterAlign = true;
			DrawStringFO2(data, "...");
		}
		if (scroll == nMenuYSize + 1) {
			tNyaStringData data;
			data.x = 0.5;
			data.y = fMenuYTop;
			data.size = 0.03;
			data.y += data.size * scroll;
			data.XCenterAlign = true;
			DrawStringFO2(data, "...");
		}
		return false;
	}

	tNyaStringData data;
	data.x = 0.5;
	data.y = fMenuYTop;
	data.size = 0.03;
	data.y += data.size * scroll;
	data.XCenterAlign = true;
	auto selected = nTempOptionCounter == nSelectedOption;
	nTempOptionCounter++;
	if (selected) {
		data.SetColor(241, 193, 45, 255);
	}
	else if (grayedOut) {
		data.SetColor(255, 255, 255, 255);
	}
	else {
		data.SetColor(127, 127, 127, 255);
	}
	DrawStringFO2(data, string);
	return selected && IsKeyJustPressed(VK_RETURN);
}

void DisableKeyboardInput(bool disable) {
	NyaHookLib::Patch<uint64_t>(0x5AEB2F, disable ? 0x68A190000001BCE9 : 0x68A1000001BB8C0F);
}

void ResetMenuScroll() {
	nMenuScroll = 0;
	nSelectedOption = 0;
}

void SetMenuScroll() {
	int tmp = nMenuScroll + nMenuYSize;
	while (tmp < nSelectedOption) {
		nMenuScroll++;
		tmp = nMenuScroll + nMenuYSize;
	}
	while (nMenuScroll > nSelectedOption) {
		nMenuScroll--;
	}
}

const char* GetPropertyValueForPreview(LiteDb* node, const char* propName) {
	static std::string str;
	str = "";
	auto type = node->GetPropertyType(propName);
	auto arraySize = node->GetPropertyArraySize(propName);
	if (type == DBVALUE_INT) {
		for (int i = 0; i < arraySize; i++) {
			if (i != 0) str += ", ";
			str += std::to_string(pCurrentNode->GetPropertyAsInt(propName, i));
		}
	}
	else if (type == DBVALUE_BOOL) {
		for (int i = 0; i < arraySize; i++) {
			if (i != 0) str += ", ";
			str += std::to_string(pCurrentNode->GetPropertyAsBool(propName, i));
		}
	}
	else if (type == DBVALUE_FLOAT) {
		for (int i = 0; i < arraySize; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", pCurrentNode->GetPropertyAsFloat(propName, i));
		}
	}
	else if (type == DBVALUE_VECTOR2 && arraySize == 1) {
		float v[2];
		pCurrentNode->GetPropertyAsVector2(&v, propName, 0);
		for (int i = 0; i < 2; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", v[i]);
		}
	}
	else if (type == DBVALUE_VECTOR3 && arraySize == 1) {
		float v[3];
		pCurrentNode->GetPropertyAsVector3(&v, propName, 0);
		for (int i = 0; i < 3; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", v[i]);
		}
	}
	else if (type == DBVALUE_VECTOR4 && arraySize == 1) {
		float v[4];
		pCurrentNode->GetPropertyAsVector4(&v, propName, 0);
		for (int i = 0; i < 4; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", v[i]);
		}
	}
	else if (type == DBVALUE_STRING) {
		str = pCurrentNode->GetPropertyAsString(propName);
	}
	else if (type == DBVALUE_NODE && arraySize == 1) {
		char path[256] = "";
		pCurrentNode->GetPropertyAsNode(propName, 0)->GetFullPath(path);
		str = path;
	}
	else {
		str = std::format("({}{})", GetPropertyTypeString(type), arraySize > 1 ? " array" : "");
	}
	return str.c_str();
}

void EnterPropertyEditor(LiteDb* node, const char* propName, int offset) {
	auto type = node->GetPropertyType(propName);
	pCurrentPropertyEditing = (void*)((uintptr_t)node->GetPropertyPointer(propName) + offset * GetPropertyTypeSize(type, true));
	ResetMenuScroll();
	nSelectedOption = 1;
	switch (type) {
		case DBVALUE_CHAR:
			sCurrentPropertyEditString = std::to_string((int)*(uint8_t*)pCurrentPropertyEditing);
			break;
		case DBVALUE_STRING:
			sCurrentPropertyEditString = (char*)pCurrentPropertyEditing;
			break;
		case DBVALUE_BOOL:
			sCurrentPropertyEditString = std::to_string(*(int*)pCurrentPropertyEditing);
			break;
		case DBVALUE_INT:
			sCurrentPropertyEditString = std::to_string(*(int*)pCurrentPropertyEditing);
			break;
		case DBVALUE_FLOAT:
		case DBVALUE_VECTOR2:
		case DBVALUE_VECTOR3:
		case DBVALUE_VECTOR4:
			sCurrentPropertyEditString = std::format("{}", *(float*)pCurrentPropertyEditing);
			break;
		case DBVALUE_RGBA:
		case DBVALUE_NODE:
		default:
			break;
	}
}

void DBEditorLoop() {
	static CNyaTimer gTimer;
	gTimer.Process();
	static double fHoldMoveTimer = 0;

	if (IsKeyJustPressed(VK_F5)) {
		DisableKeyboardInput(false);
		bMenuUp = !bMenuUp;
	}
	if (!bMenuUp) return;
	DisableKeyboardInput(true);

	if (!pCurrentNode) pCurrentNode = GetLiteDB();
	if (!pCurrentNode) return;

	if (IsKeyJustPressed(VK_ESCAPE)) {
		if (pCurrentPropertyEditing) {
			pCurrentPropertyEditing = nullptr;
		}
		else if (sCurrentProperty) {
			sCurrentProperty = nullptr;
		}
		else {
			pCurrentNode = pCurrentNode->GetParent();
			if (!pCurrentNode) pCurrentNode = GetLiteDB();
		}
		ResetMenuScroll();
		nSelectedOption = 2;
	}
	if (IsKeyPressed(VK_UP)) {
		if (IsKeyJustPressed(VK_UP)) {
			nSelectedOption--;
			fHoldMoveTimer = 0;
		}
		fHoldMoveTimer += gTimer.fDeltaTime;
		if (fHoldMoveTimer > 0.2) {
			nSelectedOption--;
			fHoldMoveTimer -= 0.2;
		}
	}
	if (IsKeyPressed(VK_DOWN)) {
		if (IsKeyJustPressed(VK_DOWN)) {
			nSelectedOption++;
			fHoldMoveTimer = 0;
		}
		fHoldMoveTimer += gTimer.fDeltaTime;
		if (fHoldMoveTimer > 0.2) {
			nSelectedOption++;
			fHoldMoveTimer -= 0.2;
		}
	}
	if (nSelectedOption < 0) nSelectedOption = nTempOptionCounter - 1;
	if (nSelectedOption >= nTempOptionCounter) nSelectedOption = 0;
	SetMenuScroll();

	nTempOptionCounter = 0;
	sEnterHint = "";

	if (pCurrentPropertyEditing) {
		auto type = pCurrentNode->GetPropertyType(sCurrentProperty);
		auto base = (uintptr_t)pCurrentNode->GetPropertyPointer(sCurrentProperty);
		auto current = (uintptr_t)pCurrentPropertyEditing;
		if (type == DBVALUE_STRING) {
			DrawMenuOption(std::format("Current Property: {}", sCurrentProperty), true);
		}
		else {
			int offset = (current - base) / GetPropertyTypeSize(type, true);
			DrawMenuOption(std::format("Current Property: {}[{}]", sCurrentProperty, offset), true);
		}

		if (nSelectedOption == nTempOptionCounter) {
			int maxLen = type == DBVALUE_STRING ? pCurrentNode->GetPropertyArraySize(sCurrentProperty) : 1024;
			char tmp[1024];
			strcpy_s(tmp, maxLen, sCurrentPropertyEditString.c_str());
			AddTextInputToString(tmp, maxLen, type != DBVALUE_STRING);
			sCurrentPropertyEditString = tmp;
			sEnterHint = "Apply Changes";
		}

		if (DrawMenuOption(sCurrentPropertyEditString + "...") && !sCurrentPropertyEditString.empty()) {
			switch (type) {
				case DBVALUE_STRING:
					strcpy_s((char*)pCurrentPropertyEditing, pCurrentNode->GetPropertyArraySize(sCurrentProperty), sCurrentPropertyEditString.c_str());
					break;
				case DBVALUE_BOOL:
				case DBVALUE_INT:
					*(int*)pCurrentPropertyEditing = std::stoi(sCurrentPropertyEditString);
					break;
				case DBVALUE_FLOAT:
				case DBVALUE_VECTOR2:
				case DBVALUE_VECTOR3:
				case DBVALUE_VECTOR4:
					*(float*)pCurrentPropertyEditing = std::stof(sCurrentPropertyEditString);
					break;
				case DBVALUE_CHAR:
					*(uint8_t*)pCurrentPropertyEditing = std::stoi(sCurrentPropertyEditString);
					break;
				default:
					break;
			}
			NyaHookLib::Patch(0x5B32D2 + 1, "PropertyDb: Table '%s' not found!");
			pCurrentPropertyEditing = nullptr;
			ResetMenuScroll();
		}
	}
	else if (sCurrentProperty) {
		DrawMenuOption(std::format("Current Property: {}", sCurrentProperty), true);
		auto propName = sCurrentProperty;
		auto type = pCurrentNode->GetPropertyType(propName);
		auto arraySize = pCurrentNode->GetPropertyArraySize(propName);
		if (nSelectedOption > 0) sEnterHint = "Edit";
		if (type == DBVALUE_INT) {
			for (int i = 0; i < arraySize; i++) {
				if (DrawMenuOption(std::format("{}: {}", i + 1, pCurrentNode->GetPropertyAsInt(propName, i)))) {
					EnterPropertyEditor(pCurrentNode, propName, i);
				}
			}
		}
		else if (type == DBVALUE_BOOL) {
			for (int i = 0; i < arraySize; i++) {
				if (DrawMenuOption(std::format("{}: {}", i + 1, pCurrentNode->GetPropertyAsBool(propName, i)))) {
					EnterPropertyEditor(pCurrentNode, propName, i);
				}
			}
		}
		else if (type == DBVALUE_CHAR) {
			for (int i = 0; i < arraySize; i++) {
				if (DrawMenuOption(std::format("{}: {}", i + 1, pCurrentNode->GetPropertyAsChar(propName, i)))) {
					EnterPropertyEditor(pCurrentNode, propName, i);
				}
			}
		}
		else if (type == DBVALUE_FLOAT) {
			for (int i = 0; i < arraySize; i++) {
				if (DrawMenuOption(std::format("{}: {}", i + 1, pCurrentNode->GetPropertyAsFloat(propName, i)))) {
					EnterPropertyEditor(pCurrentNode, propName, i);
				}
			}
		}
		else if (type == DBVALUE_VECTOR2 || type == DBVALUE_VECTOR3 || type == DBVALUE_VECTOR4) {
			auto p = (float*)pCurrentNode->GetPropertyPointer(propName);
			int count;
			if (type == DBVALUE_VECTOR2) count = 2;
			if (type == DBVALUE_VECTOR3) count = 3;
			if (type == DBVALUE_VECTOR4) count = 4;
			for (int i = 0; i < arraySize; i++) {
				for (int j = 0; j < count; j++) {
					const char* endTypes[] = {"x","y","z","w"};
					if (DrawMenuOption(std::format("{}.{}: {}", i + 1, endTypes[j], p[i*count+j]))) {
						EnterPropertyEditor(pCurrentNode, propName, i*count+j);
					}
				}
			}
		}
		else if (type == DBVALUE_STRING) {
			if (DrawMenuOption(pCurrentNode->GetPropertyAsString(propName))) {
				EnterPropertyEditor(pCurrentNode, propName, 0);
			}
		}
		else if (type == DBVALUE_NODE) {
			for (int i = 0; i < arraySize; i++) {
				if (nSelectedOption == nTempOptionCounter) sEnterHint = "Visit";

				char path[256] = "";
				auto node = pCurrentNode->GetPropertyAsNode(propName, i);
				node->GetFullPath(path);
				if (DrawMenuOption(std::format("{}: {}", i + 1, path))) {
					pCurrentNode = node;
					sCurrentProperty = nullptr;
					ResetMenuScroll();
					nSelectedOption = 2;
				}
			}
		}
		else {
			DrawMenuOption(std::format("Type {} is not currently supported", GetPropertyTypeString(type)));
			sEnterHint = "";
		}
	}
	else {
		char path[256] = "";
		pCurrentNode->GetFullPath(path);
		DrawMenuOption(std::format("Current Node: {}", path[0] ? path : "root"), true);

		if (auto numChildren = pCurrentNode->GetNumChildren()) {
			DrawMenuOption(std::format("Child Nodes: {}", numChildren), true);
			for (int i = numChildren - 1; i >= 0; i--) {
				if (nSelectedOption == nTempOptionCounter) sEnterHint = "Select";

				auto child = pCurrentNode->GetChildByIndex(i);
				if (DrawMenuOption(child->GetName())) {
					pCurrentNode = child;
					ResetMenuScroll();
					nSelectedOption = 2;
					return;
				}
			}
		}

		if (auto numProperties = pCurrentNode->GetNumProperties()) {
			DrawMenuOption(std::format("Properties: {}", numProperties), true);
			for (int i = numProperties - 1; i >= 0; i--) {
				auto propName = pCurrentNode->GetPropertyNameByIndex(i);
				if (!pCurrentNode->DoesPropertyExist(propName)) continue;
				if (nSelectedOption == nTempOptionCounter) sEnterHint = "Select";

				if (DrawMenuOption(std::format("{} - {}", propName, GetPropertyValueForPreview(pCurrentNode, propName)))) {
					sCurrentProperty = propName;
					ResetMenuScroll();
					nSelectedOption = 1;
					return;
				}
			}
		}
	}

	// menu prompts
	{
		tNyaStringData data;
		data.x = 0.5;
		data.y = fMenuYTop;
		data.size = 0.03;
		int max = nTempOptionCounter;
		if (max > nMenuYSize + 1) max = nMenuYSize + 1;
		data.y += data.size * (max + 1);
		data.XCenterAlign = true;
		std::string str;
		if (pCurrentNode->GetParent()) str += "[ESC] Back";
		if (!sEnterHint.empty()) {
			if (!str.empty()) str += " ";
			str += "[ENTER] " + sEnterHint;
		}
		DrawStringFO2(data, str);
	}
}

void HookLoop() {
	DBEditorLoop();
	CommonMain();
}

void UpdateD3DProperties() {
	g_pd3dDevice = *(IDirect3DDevice9**)(0x7242B0 + 0x60);
	ghWnd = *(HWND*)(0x7242B0 + 0x7C);
	nResX = *(int*)0x764A84;
	nResY = *(int*)0x764A88;
}

bool bDeviceJustReset = false;
void D3DHookMain() {
	if (!g_pd3dDevice) {
		UpdateD3DProperties();
		InitHookBase();
	}

	if (bDeviceJustReset) {
		ImGui_ImplDX9_CreateDeviceObjects();
		bDeviceJustReset = false;
	}
	HookBaseLoop();
}

void OnEndScene() {
	*(float*)0x716034 = 480.1f; // hack to fix font scale in chloe collection
	D3DHookMain();
	*(float*)0x716034 = 480.0f;
}

void OnD3DReset() {
	if (g_pd3dDevice) {
		UpdateD3DProperties();
		ImGui_ImplDX9_InvalidateDeviceObjects();
		bDeviceJustReset = true;
	}
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x24CEF7) {
				MessageBoxA(nullptr, aFOUCVersionFail, "nya?!~", MB_ICONERROR);
				exit(0);
				return TRUE;
			}

			NyaFO2Hooks::PlaceD3DHooks();
			NyaFO2Hooks::aEndSceneFuncs.push_back(OnEndScene);
			NyaFO2Hooks::aD3DResetFuncs.push_back(OnD3DReset);
			NyaFO2Hooks::PlaceWndProcHook();
			NyaFO2Hooks::aWndProcFuncs.push_back(WndProcHook);
		} break;
		default:
			break;
	}
	return TRUE;
}