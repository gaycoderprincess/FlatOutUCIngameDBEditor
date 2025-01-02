#include <windows.h>
#include <d3d9.h>
#include <format>
#include "toml++/toml.hpp"

#include "nya_commonmath.h"
#include "nya_commonhooklib.h"

#include "fouc.h"
#include "fo2versioncheck.h"
#include "chloemenulib.h"

uint32_t nodeVtable = 0x6F3DCC;
void DBEditor_PatchOnApply() {
	NyaHookLib::Patch(0x5B32D2 + 1, "PropertyDb: Table '%s' not found!");
}

#include "dbeditor.h"

BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID) {
	switch( fdwReason ) {
		case DLL_PROCESS_ATTACH: {
			DoFlatOutVersionCheck(FO2Version::FOUC_GFWL);
			ChloeMenuLib::RegisterMenu("In-Game DB Editor - gaycoderprincess", DBEditorLoop);
		} break;
		default:
			break;
	}
	return TRUE;
}