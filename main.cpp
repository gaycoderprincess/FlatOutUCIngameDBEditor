#include <windows.h>
#include <d3d9.h>
#include <format>
#include "toml++/toml.hpp"

#include "nya_commonmath.h"
#include "nya_commonhooklib.h"

#include "fouc.h"
#include "chloemenulib.h"

LiteDb* pCurrentPropertyEditingNodeTemp = nullptr;
std::string sCurrentPropertyEditString;
bool bPropertyEditReady = false;
bool bInPropertyEditorThisFrame = false;

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

const char* GetPropertyValueForPreview(LiteDb* node, const char* propName) {
	static std::string str;
	str = "";
	auto type = node->GetPropertyType(propName);
	auto arraySize = node->GetPropertyArraySize(propName);
	if (type == DBVALUE_INT) {
		for (int i = 0; i < arraySize; i++) {
			if (i != 0) str += ", ";
			str += std::to_string(node->GetPropertyAsInt(propName, i));
		}
	}
	else if (type == DBVALUE_BOOL) {
		for (int i = 0; i < arraySize; i++) {
			if (i != 0) str += ", ";
			str += std::to_string(node->GetPropertyAsBool(propName, i));
		}
	}
	else if (type == DBVALUE_FLOAT) {
		for (int i = 0; i < arraySize; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", node->GetPropertyAsFloat(propName, i));
		}
	}
	else if (type == DBVALUE_VECTOR2 && arraySize == 1) {
		float v[2];
		node->GetPropertyAsVector2(&v, propName, 0);
		for (int i = 0; i < 2; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", v[i]);
		}
	}
	else if (type == DBVALUE_VECTOR3 && arraySize == 1) {
		float v[3];
		node->GetPropertyAsVector3(&v, propName, 0);
		for (int i = 0; i < 3; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", v[i]);
		}
	}
	else if (type == DBVALUE_VECTOR4 && arraySize == 1) {
		float v[4];
		node->GetPropertyAsVector4(&v, propName, 0);
		for (int i = 0; i < 4; i++) {
			if (i != 0) str += ", ";
			str += std::format("{}", v[i]);
		}
	}
	else if (type == DBVALUE_STRING) {
		str += std::format("\"{}\"", node->GetPropertyAsString(propName));
	}
	else if (type == DBVALUE_NODE && arraySize == 1) {
		if (auto propNode = node->GetPropertyAsNode(propName, 0)) {
			char path[256] = "";
			propNode->GetFullPath(path);
			str = path;
		}
		else {
			str = std::format("*INVALID NODE*");
		}
	}
	else {
		str = std::format("({}{})", GetPropertyTypeString(type), arraySize > 1 ? " array" : "");
	}
	return str.c_str();
}

void DBPropertyEditLoop(LiteDb* node, const char* propName, void* pCurrentPropertyEditing) {
	ChloeMenuLib::BeginMenu();
	auto type = node->GetPropertyType(propName);
	auto base = (uintptr_t)node->GetPropertyPointer(propName);
	auto current = (uintptr_t)pCurrentPropertyEditing;
	if (type == DBVALUE_STRING) {
		DrawMenuOption(std::format("Current Property: {}", propName), true);
	}
	else {
		int offset = (current - base) / GetPropertyTypeSize(type, true);
		DrawMenuOption(std::format("Current Property: {}[{}]", propName, offset + 1), true);
	}

	if (type == DBVALUE_NODE) {
		auto bak = pCurrentPropertyEditingNodeTemp;
		if (bak < LiteDb::gNodes || *(uintptr_t*)bak != 0x6F3DCC) {
			bak = pCurrentPropertyEditingNodeTemp = &LiteDb::gNodes[0];
		}
		if (ChloeMenuLib::GetMoveLeft()) pCurrentPropertyEditingNodeTemp--;
		if (ChloeMenuLib::GetMoveRight()) pCurrentPropertyEditingNodeTemp++;

		// scrolled over an invalid node
		if (pCurrentPropertyEditingNodeTemp < LiteDb::gNodes || *(uintptr_t*)pCurrentPropertyEditingNodeTemp != 0x6F3DCC) pCurrentPropertyEditingNodeTemp = bak;
		ChloeMenuLib::SetLRScrollHint("Scroll");

		char tmp[256];
		pCurrentPropertyEditingNodeTemp->GetFullPath(tmp);
		sCurrentPropertyEditString = tmp;
		if (sCurrentPropertyEditString.empty()) sCurrentPropertyEditString = "root";
	}
	else {
		int maxLen = type == DBVALUE_STRING ? node->GetPropertyArraySize(propName) : 1024;
		char tmp[1024];
		strcpy_s(tmp, maxLen, sCurrentPropertyEditString.c_str());
		ChloeMenuLib::AddTextInputToString(tmp, maxLen, type != DBVALUE_STRING);
		sCurrentPropertyEditString = tmp;
	}
	ChloeMenuLib::SetEnterHint("Apply Changes");

	if (DrawMenuOption(type == DBVALUE_NODE ? ("< " + sCurrentPropertyEditString + " >") : (sCurrentPropertyEditString + "..."), false, false) && !sCurrentPropertyEditString.empty()) {
		switch (type) {
			case DBVALUE_STRING:
				strcpy_s((char*)pCurrentPropertyEditing, node->GetPropertyArraySize(propName), sCurrentPropertyEditString.c_str());
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
			case DBVALUE_NODE:
				*(uint16_t*)pCurrentPropertyEditing = pCurrentPropertyEditingNodeTemp - LiteDb::gNodes;
				break;
			default:
				break;
		}
		NyaHookLib::Patch(0x5B32D2 + 1, "PropertyDb: Table '%s' not found!");
		ChloeMenuLib::BackOut();
	}
	ChloeMenuLib::EndMenu();
}

void EnterPropertyEditor(LiteDb* node, const char* propName, int offset) {
	bInPropertyEditorThisFrame = true;
	auto type = node->GetPropertyType(propName);
	auto pCurrentPropertyEditing = (void*)((uintptr_t)node->GetPropertyPointer(propName) + offset * GetPropertyTypeSize(type, true));
	if (!bPropertyEditReady) {
		switch (type) {
			case DBVALUE_CHAR:
				sCurrentPropertyEditString = std::to_string((int) *(uint8_t *) pCurrentPropertyEditing);
				break;
			case DBVALUE_STRING:
				sCurrentPropertyEditString = (char *) pCurrentPropertyEditing;
				break;
			case DBVALUE_BOOL:
				sCurrentPropertyEditString = std::to_string(*(int *) pCurrentPropertyEditing);
				break;
			case DBVALUE_INT:
				sCurrentPropertyEditString = std::to_string(*(int *) pCurrentPropertyEditing);
				break;
			case DBVALUE_FLOAT:
			case DBVALUE_VECTOR2:
			case DBVALUE_VECTOR3:
			case DBVALUE_VECTOR4:
				sCurrentPropertyEditString = std::format("{}", *(float *) pCurrentPropertyEditing);
				break;
			case DBVALUE_NODE:
				pCurrentPropertyEditingNodeTemp = &LiteDb::gNodes[*(uint16_t *) pCurrentPropertyEditing];
				break;
			case DBVALUE_RGBA:
			default:
				break;
		}
		bPropertyEditReady = true;
	}

	DBPropertyEditLoop(node, propName, pCurrentPropertyEditing);
}

void DBEditorPropertyLoop(LiteDb* node, const char* propName) {
	bInPropertyEditorThisFrame = false;

	ChloeMenuLib::BeginMenu();
	DrawMenuOption(std::format("Current Property: {}",  propName), true);
	auto type = node->GetPropertyType(propName);
	auto arraySize = node->GetPropertyArraySize(propName);
	ChloeMenuLib::SetEnterHint("Edit");
	if (type == DBVALUE_INT) {
		for (int i = 0; i < arraySize; i++) {
			if (DrawMenuOption(std::format("{}: {}", i + 1, node->GetPropertyAsInt(propName, i)))) {
				EnterPropertyEditor(node, propName, i);
			}
		}
	}
	else if (type == DBVALUE_BOOL) {
		for (int i = 0; i < arraySize; i++) {
			if (DrawMenuOption(std::format("{}: {}", i + 1, node->GetPropertyAsBool(propName, i)))) {
				EnterPropertyEditor(node, propName, i);
			}
		}
	}
	else if (type == DBVALUE_CHAR) {
		for (int i = 0; i < arraySize; i++) {
			if (DrawMenuOption(std::format("{}: {}", i + 1, node->GetPropertyAsChar(propName, i)))) {
				EnterPropertyEditor(node, propName, i);
			}
		}
	}
	else if (type == DBVALUE_FLOAT) {
		for (int i = 0; i < arraySize; i++) {
			if (DrawMenuOption(std::format("{}: {}", i + 1, node->GetPropertyAsFloat(propName, i)))) {
				EnterPropertyEditor(node, propName, i);
			}
		}
	}
	else if (type == DBVALUE_VECTOR2 || type == DBVALUE_VECTOR3 || type == DBVALUE_VECTOR4) {
		auto p = (float*)node->GetPropertyPointer(propName);
		int count;
		if (type == DBVALUE_VECTOR2) count = 2;
		if (type == DBVALUE_VECTOR3) count = 3;
		if (type == DBVALUE_VECTOR4) count = 4;
		for (int i = 0; i < arraySize; i++) {
			for (int j = 0; j < count; j++) {
				const char* endTypes[] = {"x","y","z","w"};
				if (DrawMenuOption(std::format("{}.{}: {}", i + 1, endTypes[j], p[i*count+j]))) {
					EnterPropertyEditor(node, propName, i*count+j);
				}
			}
		}
	}
	else if (type == DBVALUE_STRING) {
		if (DrawMenuOption(std::format("1: \"{}\"", node->GetPropertyAsString(propName)))) {
			EnterPropertyEditor(node, propName, 0);
		}
	}
	else if (type == DBVALUE_NODE) {
		for (int i = 0; i < arraySize; i++) {
			char path[256];
			if (auto propNode = node->GetPropertyAsNode(propName, i)) {
				propNode->GetFullPath(path);
			}
			else {
				strcpy_s(path, 256, "*INVALID NODE*");
			}
			if (DrawMenuOption(std::format("{}: {}", i + 1, path))) {
				EnterPropertyEditor(node, propName, i);
			}
		}
	}
	else {
		DrawMenuOption(std::format("Type {} is not currently supported", GetPropertyTypeString(type)), false, false);
		ChloeMenuLib::SetEnterHint("");
	}
	ChloeMenuLib::EndMenu();

	if (!bInPropertyEditorThisFrame) {
		bPropertyEditReady = false;
	}
}

void DBEditorTreeLoop(LiteDb* node) {
	ChloeMenuLib::BeginMenu();
	char path[256] = "";
	node->GetFullPath(path);
	DrawMenuOption(std::format("Current Node: {}", path[0] ? path : "root"), true);

	if (auto numChildren = node->GetNumChildren()) {
		DrawMenuOption(std::format("Child Nodes: {}", numChildren), true);
		for (int i = numChildren - 1; i >= 0; i--) {
			auto child = node->GetChildByIndex(i);
			if (DrawMenuOption(child->GetName())) {
				DBEditorTreeLoop(child);
			}
		}
	}

	if (auto numProperties = node->GetNumProperties()) {
		DrawMenuOption(std::format("Properties: {}", numProperties), true);
		for (int i = 0; i < numProperties; i++) {
			auto propName = node->GetPropertyNameByIndex(i);
			if (!node->DoesPropertyExist(propName)) continue;

			if (DrawMenuOption(std::format("{} - {}", propName, GetPropertyValueForPreview(node, propName)))) {
				DBEditorPropertyLoop(node, propName);
			}
		}
	}
	ChloeMenuLib::EndMenu();
}

void DBEditorLoop() {
	DBEditorTreeLoop(GetLiteDB());
}

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			if (NyaHookLib::GetEntryPoint() != 0x24CEF7) {
				MessageBoxA(nullptr, aFOUCVersionFail, "nya?!~", MB_ICONERROR);
				exit(0);
				return TRUE;
			}

			ChloeMenuLib::RegisterMenu("In-Game DB Editor - gaycoderprincess", DBEditorLoop);
		} break;
		default:
			break;
	}
	return TRUE;
}