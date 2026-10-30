//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Speak extension
// SPDX-License-Identifier: MIT
// ===========================================================================
// Windows speech backend (SAPI).
//

#define _WIN32_WINNT 0x0602 // Set to Windows 8 or later
#define NTDDI_VERSION 0x06020000 // Set to Windows 8 or later
#pragma warning(disable : 4996)

#include <iostream>
#include <locale>
#include <sapi.h>
#include <sphelper.h> // Include this for CLSID_SpVoice and other helpers

// System headers above may define their own BYTE_SIZE; Rebol's must win here.
#undef BYTE_SIZE
#include "gen-speak.h"
#include "speak.h"

extern "C" {

	void list_voices(void) {
		// Enumerate available voices
		IEnumSpObjectTokens *pEnum = NULL;
		ULONG ulCount = 0;

		HRESULT hr = SpEnumTokens(SPCAT_VOICES, NULL, NULL, &pEnum);
		if (FAILED(hr)) return;

		// Get the number of voices
		hr = pEnum->GetCount(&ulCount);
		// If there are voices available, let's list them
		if (SUCCEEDED(hr) && ulCount > 0) {
			puts("Available Voices:");
			// Loop through all available voices
			for (ULONG i = 0; i < ulCount; i++) {
				ISpObjectToken *pToken = NULL;
				hr = pEnum->Item(i, &pToken);
				if (SUCCEEDED(hr)) {
					// Get the name of the voice
					LPWSTR pwszName = NULL;
					hr = SpGetDescription(pToken, &pwszName);
					if (SUCCEEDED(hr)) {
						// 1-based, so that the printed number can be used
						// directly as `say/as text <number>`
						std::wcout << (i + 1) << L": " << pwszName << std::endl;
						::CoTaskMemFree(pwszName);
					}
					pToken->Release();
				}
			}
		}
		pEnum->Release();
	}

	int speak(voice_t* voice, int no_wait) {
		HRESULT hr;
		ISpVoice *pVoice = NULL;

		if (voice->synth) {
			pVoice = (ISpVoice*)voice->synth;
		} else {
			hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
			if (FAILED(hr)) return 0;
			voice->synth = pVoice;
		}

		if (voice->number > 0) {
			IEnumSpObjectTokens *pEnum = NULL;
			ULONG ulCount = 0;
			hr = SpEnumTokens(SPCAT_VOICES, NULL, NULL, &pEnum);
			if (SUCCEEDED(hr)) {
				ISpObjectToken *pSelectedVoice = NULL;
				hr = pEnum->Item(voice->number-1, &pSelectedVoice);
				if (SUCCEEDED(hr)) {
					pVoice->SetVoice(pSelectedVoice);
					pSelectedVoice->Release();
				}
				pEnum->Release();
			}
		}
		DWORD flags = SPF_PURGEBEFORESPEAK | SPF_IS_XML | SPF_PERSIST_XML;
		if (no_wait) flags |= SPF_ASYNC;
		hr = pVoice->Speak((WCHAR*)voice->text, flags , NULL);
//		if (!no_wait) {
//			// Wait for the speech to complete
//			if (SUCCEEDED(hr)) {
//				// Block until the speech is completed
//				pVoice->WaitUntilDone(INFINITE);
//			}
//		}
		return 0;
	}

	void release_voice(voice_t* voice) {
		if (voice == NULL) return;
		ISpVoice *pVoice = (ISpVoice*)voice->synth;
		if (pVoice) {
			pVoice->Release();
			voice->synth = NULL;
		}
	}
}
