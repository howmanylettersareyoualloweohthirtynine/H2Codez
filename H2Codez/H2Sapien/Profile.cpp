#include "Profile.h"
#include "../stdafx.h"
#include <Wincrypt.h>
#include "../Common/FiloInterface.h"
#include "../util/Patches.h"
#include "../Tags/MultiplayerGlobals.h"

void DuplicateDataBlob(DATA_BLOB  *pDataIn, DATA_BLOB  *pDataOut)
{
	pDataOut->cbData = pDataIn->cbData;
	pDataOut->pbData = static_cast<BYTE*>(LocalAlloc(LMEM_FIXED, pDataIn->cbData));
	CopyMemory(pDataOut->pbData, pDataIn->pbData, pDataIn->cbData);
}

BOOL WINAPI CryptProtectDataHook(
	_In_       DATA_BLOB                 *pDataIn,
	_In_opt_   LPCWSTR                   szDataDescr,
	_In_opt_   DATA_BLOB                 *pOptionalEntropy,
	_Reserved_ PVOID                     pvReserved,
	_In_opt_   CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
	_In_       DWORD                     dwFlags,
	_Out_      DATA_BLOB                 *pDataOut
)
{
	DuplicateDataBlob(pDataIn, pDataOut);

	return TRUE;
}

auto CryptUnprotectDataOrg = CryptUnprotectData;
BOOL WINAPI CryptUnprotectDataHook(
	_In_       DATA_BLOB                 *pDataIn,
	_Out_opt_  LPWSTR                    *ppszDataDescr,
	_In_opt_   DATA_BLOB                 *pOptionalEntropy,
	_Reserved_ PVOID                     pvReserved,
	_In_opt_   CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct,
	_In_       DWORD                     dwFlags,
	_Out_      DATA_BLOB                 *pDataOut
)
{
	if (CryptUnprotectDataOrg(pDataIn, ppszDataDescr, pOptionalEntropy, pvReserved, pPromptStruct, dwFlags, pDataOut) == FALSE) {
		pLog.WriteLog("Data not encrypted");
		DuplicateDataBlob(pDataIn, pDataOut); // if decrypting the data fails just assume it's unencrypted
	}

	return TRUE;
}

char filo__write_encrypted_hook(filo *file_ptr, DWORD nNumberOfBytesToWrite, LPVOID lpBuffer)
{
	pLog.WriteLog("filo__write_encrypted_hook: filo->path %s", file_ptr->path);
	DWORD file_size = GetFileSize(file_ptr->handle, NULL);

	if (file_size > nNumberOfBytesToWrite) // clear the file as unencrypted data is shorter then encrypted data.
		FiloInterface::change_size(file_ptr, 0);
	return FiloInterface::write(file_ptr, lpBuffer, nNumberOfBytesToWrite);
}

DWORD *get_multiplayer_globals()
{
	typedef DWORD * (__cdecl *get_multiplayer_globals)();
	auto get_multiplayer_globals_impl = reinterpret_cast<get_multiplayer_globals>(0x4D5390);
	return get_multiplayer_globals_impl();
}

void *TAG_GET(int tag_group, datum tag_datum)
{
	typedef void * (__cdecl *TAG_GET)(int tag_group, datum tag_datum);
	auto TAG_GET_IMPL = reinterpret_cast<TAG_GET>(0x4B0B20);
	return TAG_GET_IMPL(tag_group, tag_datum);
}

void *TAG_BLOCK_GET_ELEMENT_WITH_SIZE(tag_block_ref *tag_block, int index, int size)
{
	typedef void * (__cdecl *TAG_BLOCK_GET_ELEMENT_WITH_SIZE)(tag_block_ref *tag_block, int index, int size);
	auto TAG_BLOCK_GET_ELEMENT_WITH_SIZE_IMPL = reinterpret_cast<TAG_BLOCK_GET_ELEMENT_WITH_SIZE>(0x004B4390);
	return TAG_BLOCK_GET_ELEMENT_WITH_SIZE_IMPL(tag_block, index, size);
}

void get_string_from_string_id(int subtitle_tag_index, int string_id, wchar_t *output)
{
	typedef void (__cdecl *get_string_from_string_id)(int subtitle_tag_index, int string_id, wchar_t *output);
	auto get_string_from_string_id_impl = reinterpret_cast<get_string_from_string_id>(0x51C1B0);
	get_string_from_string_id_impl(subtitle_tag_index, string_id, output);
}

void __cdecl get_string_id_from_multiplayer_globals(int string_id, wchar_t *Dst, int, int)
{
	DWORD *globals = get_multiplayer_globals();
	if (globals)
	{
		pLog.WriteLog("WTF no mutliplayer globals exist");
	}

	datum *mulg_datum = (datum*)&globals[116];
	if (mulg_datum->index == 0xFFFF)
	{
		pLog.WriteLog("No multiplayer globals tag");
		return;
	}
	multiplayer_globals_block *global_tag = (multiplayer_globals_block*)TAG_GET('mulg', *mulg_datum);
	if (global_tag->universal.size == 0)
	{
		pLog.WriteLog("mutliplayer globals missing universal");
		return;
	}
	multiplayer_universal_block *universal = (multiplayer_universal_block*)TAG_BLOCK_GET_ELEMENT_WITH_SIZE(&global_tag->universal, 0, sizeof(multiplayer_universal_block));
	if (LOG_CHECK(universal)) // should never be null since we checked this before
	{
		auto multi_text = universal->multiplayerText;
		if (multi_text.tag_index == 0xFFFF)
		{
			pLog.WriteLog("No multiplayer text in globals");
			return;
		}
		get_string_from_string_id(multi_text.tag_index, string_id, Dst);
		pLog.WriteLog("string_id: %d Dst: %S", string_id, Dst);
	}
}

void H2SapienPatches::fix_game_save()
{
	// allows the globals to always be setup
	NopFill(0x0051F2BA, 2);
	// disable requirement for UI unic tag to be loaded
	NopFill(0x51E607, 2);

	// don't encrypt data (sync with project cartographer)
	PatchCall(0x5D9604, filo__write_encrypted_hook);

	// default one crashes sometimes
	WriteJmp(0x585250, get_string_id_from_multiplayer_globals);

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	auto CryptProtectData_org = CryptProtectData;
	DetourAttach(&(PVOID&)CryptProtectData_org, CryptProtectDataHook);

	DetourAttach(&(PVOID&)CryptUnprotectDataOrg, CryptUnprotectDataHook);

	DetourTransactionCommit();
}