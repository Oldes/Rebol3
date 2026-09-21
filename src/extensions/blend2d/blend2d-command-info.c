//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// Project: Rebol/Blend2D extension
// SPDX-License-Identifier: Apache-2.0
// =============================================================================
// The `info` command - reports the library's runtime state, or the content of
// one of the extension's handles.
//

#include "gen-blend2d.h"
#include "blend2d-command.h"

static const REBYTE *ERR_NO_STRING     = (const REBYTE*)"Blend2D failed to make the result string!";
static const REBYTE *ERR_BAD_HANDLE    = (const REBYTE*)"Not a Blend2D handle!";

static const char* RuntimeCpuArchString(uint32_t val) {
	switch (val) {
	case BL_RUNTIME_CPU_ARCH_X86:  return "x86";
	case BL_RUNTIME_CPU_ARCH_ARM:  return "ARM";
	case BL_RUNTIME_CPU_ARCH_MIPS: return "MIPS";
	default:                       return "unknown";
	}
}

COMMAND cmd_blend2d_info(RXIFRM *frm, void *ctx) {
	REBSER *str;
	int len = 0;

	// The result string grows on demand; 512 bytes is enough for a handle and
	// close enough for the runtime report.
	str = RL_MAKE_STRING(512, FALSE);
	if (str == NULL) RETURN_ERROR(ERR_NO_STRING);
	SERIES_TAIL(str) = 0;

	if (RXA_REF(frm, 1) && RXT_HANDLE == RXA_TYPE(frm, 2)) {
		REBHOB *hob = RXA_HANDLE_CONTEXT(frm, 2);

		if (hob == NULL || !IS_USED_HOB(hob)) RETURN_ERROR(ERR_BAD_HANDLE);

		if (hob->sym == Handle_BLFontFace) {
			BLFontFaceInfo info;
			blFontFaceGetFaceInfo((BLFontFaceCore*)hob->data, &info);
			APPEND_STRING(str,
				"faceType:    %u\n"
				"outlineType: %u\n"
				"glyphCount:  %u\n"
				"revision:    %u\n"
				"faceIndex:   %u\n"
				"faceFlags:   %u\n"
				"diagFlags:   %u\n",
				(unsigned)info.faceType,
				(unsigned)info.outlineType,
				(unsigned)info.glyphCount,
				(unsigned)info.revision,
				(unsigned)info.faceIndex,
				(unsigned)info.faceFlags,
				(unsigned)info.diagFlags
			);
		}
		else if (hob->sym == Handle_BLPath) {
			BLPathCore *path = (BLPathCore*)hob->data;
			APPEND_STRING(str,
				"size:     %llu\n"
				"capacity: %llu\n",
				(unsigned long long)blPathGetSize(path),
				(unsigned long long)blPathGetCapacity(path)
			);
		}
		else if (hob->sym == Handle_BLImage) {
			BLImageData data;
			if (blImageGetData((BLImageCore*)hob->data, &data) != BL_SUCCESS) {
				RETURN_ERROR(ERR_BAD_HANDLE);
			}
			APPEND_STRING(str,
				"width:  %i\n"
				"height: %i\n"
				"stride: %lli\n"
				"format: %u\n"
				"flags:  %u\n",
				(int)data.size.w,
				(int)data.size.h,
				(long long)data.stride,
				(unsigned)data.format,
				(unsigned)data.flags
			);
		}
		else {
			RETURN_ERROR(ERR_BAD_HANDLE);
		}
	}
	else {
		BLRuntimeBuildInfo buildInfo;
		BLRuntimeResourceInfo resourceInfo;
		BLRuntimeSystemInfo systemInfo;

		blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_BUILD, &buildInfo);
		blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_SYSTEM, &systemInfo);
		blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_RESOURCE, &resourceInfo);

		APPEND_STRING(str,
			"Version:     %u.%u.%u\n"
			"Build-type:  %s\n"
			"Compiled-by: %s\n"
			"Threads:     %u\n\n",
			(unsigned)buildInfo.majorVersion, (unsigned)buildInfo.minorVersion, (unsigned)buildInfo.patchVersion,
			buildInfo.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
			buildInfo.compilerInfo,
			(unsigned)Blend2D_thread_count
		);
		APPEND_STRING(str,
			"System: [\n"
			"  cpuArch:      %s\n"      //! Host CPU architecture, see `BLRuntimeCpuArch`.
			"  cpuFeatures:  %u\n"      //! Host CPU features, see `BLRuntimeCpuFeatures`.
			"  coreCount:    %u\n"      //! Number of cores of the host CPU/CPUs.
			"  threadCount:  %u\n"      //! Number of threads of the host CPU/CPUs.
			"  threadStackSize:       %u\n" //! Minimum stack size of a worker thread used by Blend2D.
			"  allocationGranularity: %u\n" //! Allocation granularity of virtual memory (includes thread's stack).
			"]\n\n",
			RuntimeCpuArchString(systemInfo.cpuArch),
			(unsigned)systemInfo.cpuFeatures,
			(unsigned)systemInfo.coreCount,
			(unsigned)systemInfo.threadCount,
			(unsigned)systemInfo.threadStackSize,
			(unsigned)systemInfo.allocationGranularity
		);
		APPEND_STRING(str,
			"Resource: [\n"
			"  vmUsed:       %llu\n"
			"  vmReserved:   %llu\n"
			"  vmOverhead:   %llu\n"
			"  vmBlockCount: %llu\n"
			"  zmUsed:       %llu\n"
			"  zmReserved:   %llu\n"
			"  zmOverhead:   %llu\n"
			"  zmBlockCount: %llu\n"
			"  dynamicPipelineCount: %llu\n"
			"]\n",
			(unsigned long long)resourceInfo.vmUsed,
			(unsigned long long)resourceInfo.vmReserved,
			(unsigned long long)resourceInfo.vmOverhead,
			(unsigned long long)resourceInfo.vmBlockCount,
			(unsigned long long)resourceInfo.zmUsed,
			(unsigned long long)resourceInfo.zmReserved,
			(unsigned long long)resourceInfo.zmOverhead,
			(unsigned long long)resourceInfo.zmBlockCount,
			(unsigned long long)resourceInfo.dynamicPipelineCount
		);
	}

	RXA_SERIES(frm, 1) = str;
	RXA_INDEX(frm, 1) = 0;
	RXA_TYPE(frm, 1) = RXT_STRING;
	return RXR_VALUE;
}