//   ____  __   __        ______        __
//  / __ \/ /__/ /__ ___ /_  __/__ ____/ /
// / /_/ / / _  / -_|_-<_ / / / -_) __/ _ \
// \____/_/\_,_/\__/___(@)_/  \__/\__/_// /
//  ~~~ oldes.huhuman at gmail.com ~~~ /_/
//
// SPDX-License-Identifier: MIT
// =============================================================================
// Rebol/MiniAudio extension commands
// =============================================================================


#include "gen-miniaudio.h"
#include "miniaudio-extension.h"
#include <stdio.h>
#include <stdlib.h> // malloc
#include <math.h>   // fmin, fmax

// COMMAND, FRM_IS_HANDLE, ARG_Double/Float/Int32, ARG_Handle_Series,
// RETURN_ERROR, RETURN_HANDLE, APPEND_STRING and the trace macros are all
// provided by the shared reb-ext-common.h part of the extension API header.

#define ARG_Is_MAEngine(n)    FRM_IS_HANDLE(n, Handle_MAEngine)
#define ARG_Is_MASound(n)     FRM_IS_HANDLE(n, Handle_MASound)
#define ARG_Is_MANoise(n)     FRM_IS_HANDLE(n, Handle_MANoise)
#define ARG_Is_MAWaveform(n)  FRM_IS_HANDLE(n, Handle_MAWaveform)
#define ARG_Is_MAGroup(n)     FRM_IS_HANDLE(n, Handle_MAGroup)
#define ARG_MASound(n)        (ARG_Is_MASound(n)    ? (ma_sound*)(RXA_HANDLE_CONTEXT(frm, n)->handle) : NULL)
#define ARG_MAGroup(n)        (ARG_Is_MAGroup(n)    ? (ma_sound_group*)(RXA_HANDLE_CONTEXT(frm, n)->handle) : NULL)
#define ARG_MANoise(n)        (ARG_Is_MANoise(n)    ? (ma_noise*)(RXA_HANDLE_CONTEXT(frm, n)->handle) : NULL)
#define ARG_MAWaveform(n)     (ARG_Is_MAWaveform(n) ? (ma_waveform*)(RXA_HANDLE_CONTEXT(frm, n)->handle) : NULL)
#define ARG_Is_DataSource(n)  (RXA_TYPE(frm,n) == RXT_HANDLE && (RXA_HANDLE_TYPE(frm, n) == Handle_MANoise || RXA_HANDLE_TYPE(frm, n) == Handle_MAWaveform) )
#define ARG_DataSource(n)     (ma_data_source*)(RXA_HANDLE_CONTEXT(frm, n)->handle)

#define ASSERT_CONTEXT()   if(!gContextReady) RETURN_ERROR("MiniAudio context is not initialized!");
#define ASSERT_ENGINE()    if(pEngine == NULL) RETURN_ERROR("No playback device initialized!");


MAContext* pEngine = NULL;
REBHOB*    pEngineHob = NULL;
ma_context gContext;
ma_resource_manager gResourceManager;
int gContextReady = 0;


//== library startup ==========================================================

// Called once from the extension's entry point (both build modes), because
// the generated `init-words` command does nothing but map the word lists.
int MiniAudio_Startup(void) {
	ma_resource_manager_config config;

	if (gContextReady) return 1;

	config = ma_resource_manager_config_init();
	/* ma_format_f32 should almost always be used as that's what the engine
	   (and most everything else) uses for mixing. */
	config.decodedFormat     = ma_format_f32;
	/* Setting the channel count to 0 will cause sounds to use their native
	   channel count. */
	config.decodedChannels   = 0;
	/* A consistent sample rate avoids expensive resampling in the audio
	   thread - it is done by the loading thread(s) instead. */
	config.decodedSampleRate = 44100;

	if (MA_SUCCESS != ma_resource_manager_init(&config, &gResourceManager)) {
		trace("Failed to initialize the MiniAudio resource manager!");
		return 0;
	}
	if (MA_SUCCESS != ma_context_init(NULL, 0, NULL, &gContext)) {
		trace("Failed to initialize the MiniAudio context!");
		ma_resource_manager_uninit(&gResourceManager);
		return 0;
	}
	gContextReady = 1;
	return 1;
}


static ma_uint64 abs_sound_frames(RXIARG *arg, ma_sound *sound) {
	ma_engine *engine = ma_sound_get_engine(sound);
	return arg->uint64 + ma_engine_get_time_in_pcm_frames(engine);
}
static ma_uint64 abs_sound_time_to_frames(RXIARG *arg, ma_sound *sound) {
	ma_engine *engine = ma_sound_get_engine(sound);
	ma_uint64 frames = (arg->uint64 * ma_engine_get_sample_rate(engine)) / 1000000000;
	return frames + ma_engine_get_time_in_pcm_frames(engine);
}

static ma_result sound_init_from_arg(RXIARG *file, REBSER **out_ser, ma_uint32 flags, ma_sound_group* pGroup, ma_fence* pDoneFence, ma_sound* pSound) {
	REBSER *ser;
#ifdef TO_WINDOWS
	// Convert Rebol file to a full local path (wide chars)
	ser = RL_TO_LOCAL_PATH(file, 1, 0);
	if (!ser) return MA_INVALID_FILE;
	*out_ser = ser;
	return ma_sound_init_from_file_w(pEngine->engine, (const wchar_t*)SERIES_DATA(ser), 0, pGroup, NULL, pSound);
#endif
	// Posix version... the local path is requested as UTF-8 directly.
	ser = RL_TO_LOCAL_PATH(file, 1, 1);
	if (!ser) return MA_INVALID_FILE;
	*out_ser = ser;
	return ma_sound_init_from_file(pEngine->engine, (const char*)SERIES_DATA(ser), 0, pGroup, NULL, pSound);
}


static void onSoundEnd(void* hob, ma_sound* pSound) {
	trace("sound end ");
}

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	RXIARG args[4];
	MAContext* ctx;

	(void)pInput;

	ctx = (MAContext*)pDevice->pUserData;
	if (ctx == NULL || ctx->engine == NULL) return;

	if (ctx->callback.obj) {
		CLEAR(&args[0], sizeof(args));
		ctx->callback.args = args;

		// Pass a requested frame count and total engine frames count
		RXI_COUNT(args) = 2;
		RXI_TYPE(args, 1) = RXT_INTEGER;
		args[1].uint64 = frameCount;

		RXI_TYPE(args, 2) = RXT_INTEGER;
		args[2].uint64 = ma_engine_get_time_in_pcm_frames(ctx->engine);

		RL_CALLBACK(&ctx->callback);
	}

	ma_engine_read_pcm_frames(ctx->engine, pOutput, frameCount, NULL);
}


int Common_mold(REBHOB *hob, REBSER *str) {
	int len;
	if (!str) return 0;
	SERIES_TAIL(str) = 0;
	APPEND_STRING(str, "0#%lx", (unsigned long)(uintptr_t)hob->data);
	return len;
}


int MAEngine_free(void* hndl) {
	RXIARG  arg;
	REBSER *blk;
	REBHOB *hob;
	MAContext *context;

	if (!hndl) return 0;
	hob = (REBHOB *)hndl;
	context = (MAContext*)hob->data;
	if (!context || !context->engine || !context->device) return 0;
	ma_device_stop(context->device);

	debug_print("release engine: %p is referenced: %i\n", hob->data, IS_MARK_HOB(hob) != 0);
	UNMARK_HOB(hob);
	blk = hob->series;
	if (blk) {
		int i = blk->tail;
		while (i-->0) {
			if (RXT_HANDLE == RL_GET_VALUE(blk, i, &arg)) {
				RL_FREE_HANDLE_CONTEXT(arg.handle.ptr);
			}
		}
		RESET_SERIES(blk);
		hob->series = NULL;
	}
	
	ma_device_uninit(context->device);
	ma_engine_uninit(context->engine);
	free(context->engine);
	free(context->device);
	context->engine = NULL;
	context->device = NULL;
	return 0;
}
int MAEngine_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	MAContext* context = (MAContext*)hob->data;
	ma_engine* engine = context->engine;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_RESOURCES:
		*type = RXT_BLOCK;
		arg->series = hob->series;
		arg->index = 0;
		break;
	case W_MINIAUDIO_ARG_VOLUME:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_engine_get_volume(engine);
		break;
	case W_MINIAUDIO_ARG_FRAMES:
		*type = RXT_INTEGER;
		arg->uint64 = ma_engine_get_time_in_pcm_frames(engine);
		break;
	case W_MINIAUDIO_ARG_TIME:
		*type = RXT_TIME;
		arg->int64 = ma_engine_get_time_in_milliseconds(engine) * 1000000;
		break;
	case W_MINIAUDIO_ARG_CHANNELS:
		*type = RXT_INTEGER;
		arg->uint64 = ma_engine_get_channels(engine);
		break;
	case W_MINIAUDIO_ARG_SAMPLE_RATE:
		*type = RXT_INTEGER;
		arg->uint64 = ma_engine_get_sample_rate(engine);
		break;
	case W_MINIAUDIO_ARG_GAIN_DB:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_engine_get_gain_db(engine);
		break;
	default:
		return PE_BAD_SELECT;	
	}
	return PE_USE;
}
int MAEngine_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	MAContext* context = (MAContext*)hob->data;
	ma_engine* engine = context->engine;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_VOLUME:
		switch (*type) {
		case RXT_DECIMAL:
		case RXT_PERCENT:
			ma_engine_set_volume(engine, (float)arg->dec64);
			break;
		case RXT_INTEGER:
			ma_engine_set_volume(engine, (float)arg->int64);
			break;
		default: 
			return PE_BAD_SET_TYPE;
		}
		break;
	case W_MINIAUDIO_ARG_FRAMES:
		if (*type != RXT_INTEGER) return PE_BAD_SET_TYPE;
		ma_engine_set_time_in_pcm_frames(engine, arg->uint64);
		break;
	case W_MINIAUDIO_ARG_TIME:
		if (*type != RXT_TIME) return PE_BAD_SET_TYPE;
		if (arg->uint64 < 0) return PE_BAD_SET;
		ma_engine_set_time_in_milliseconds(engine, arg->uint64 / 1000000);
		break;
	case W_MINIAUDIO_ARG_GAIN_DB:
		if      (*type == RXT_DECIMAL) ma_engine_set_gain_db(engine, (float)arg->dec64);
		else if (*type == RXT_INTEGER) ma_engine_set_gain_db(engine, (float)arg->int64);
		else return PE_BAD_SET_TYPE;
		break;
	default:
		return PE_BAD_SET;	
	}
	return PE_OK;
}


int MASound_free(void* hndl) {
	REBHOB *hob;
	if (!hndl) return 0;
	hob = (REBHOB *)hndl;
	debug_print("release sound: %p is referenced: %i\n", hob->data, IS_MARK_HOB(hob) != 0);

	ma_sound *sound = (ma_sound*)hob->data;

	// Don't release it, if not referenced but still playing...
	if(!IS_MARK_HOB(hob) && ma_sound_is_playing(sound)) {
		debug_print("preventing sound release?\n");
		MARK_HOB(hob);
		return 0;
	}

	UNMARK_HOB(hob);
	ma_sound_uninit(sound);
	if (hob->series) {
		RESET_SERIES(hob->series);
		hob->series = NULL;
	}
	return 0;
}
int MASound_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_sound* sound = (ma_sound*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	ma_uint64 frames;
	ma_uint32 sampleRate;
	ma_vec3f pos;

	switch (word) {
	case W_MINIAUDIO_ARG_VOLUME:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_sound_get_volume(sound);
		break;
	case W_MINIAUDIO_ARG_POSITION:
		*type = RXT_PAIR;
		pos = ma_sound_get_position(sound);
		arg->dec32a = pos.x;
		arg->dec32b = pos.y;
		break;
	case W_MINIAUDIO_ARG_PAN:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_sound_get_pan(sound);
		break;
	case W_MINIAUDIO_ARG_PITCH:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_sound_get_pitch(sound);
		break;
	case W_MINIAUDIO_ARG_CURSOR:
		*type = RXT_INTEGER;
		ma_sound_get_cursor_in_pcm_frames(sound, &frames);
		arg->int64 = frames;
		break;
	case W_MINIAUDIO_ARG_DURATION:
		*type = RXT_TIME;
		ma_sound_get_length_in_pcm_frames(sound, &frames);
		sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
		arg->uint64 = (frames * 1000000000) / sampleRate;
		break;
	case W_MINIAUDIO_ARG_FRAMES:
		*type = RXT_INTEGER;
		ma_sound_get_length_in_pcm_frames(sound, &frames);
		arg->int64 = frames;
		break;
	case W_MINIAUDIO_ARG_SAMPLE_RATE:
		*type = RXT_INTEGER;
		arg->int64 = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
		break;
	case W_MINIAUDIO_ARG_SPATIALIZE:
		*type = RXT_LOGIC;
		arg->int32a = ma_sound_is_spatialization_enabled(sound);
		break;
	case W_MINIAUDIO_ARG_IS_LOOPING:
		*type = RXT_LOGIC;
		arg->int32a = ma_sound_is_looping(sound);
		break;
	case W_MINIAUDIO_ARG_AT_END:
		*type = RXT_LOGIC;
		arg->int32a = ma_sound_at_end(sound);
		break;
	case W_MINIAUDIO_ARG_IS_PLAYING:
		*type = RXT_LOGIC;
		arg->int32a = ma_sound_is_playing(sound);
		break;
	case W_MINIAUDIO_ARG_SOURCE:
		if (hob->series) {
			*type = RL_GET_VALUE(hob->series, 0, arg);
		}
		else *type = RXT_NONE;
		break;

	case W_MINIAUDIO_ARG_START:
		*type = RXT_INTEGER;
		arg->uint64 = ma_node_get_state_time(sound, ma_node_state_started);
		break;	
	case W_MINIAUDIO_ARG_STOP:
		*type = RXT_INTEGER;
		arg->uint64 = ma_node_get_state_time(sound, ma_node_state_stopped);
		break;

	case W_MINIAUDIO_ARG_X:
		*type = RXT_DECIMAL;
		pos = ma_sound_get_position(sound);
		arg->dec64 = pos.x;
		break;
	case W_MINIAUDIO_ARG_Y:
		*type = RXT_DECIMAL;
		pos = ma_sound_get_position(sound);
		arg->dec64 = pos.y;
		break;
	case W_MINIAUDIO_ARG_Z:
		*type = RXT_DECIMAL;
		pos = ma_sound_get_position(sound);
		arg->dec64 = pos.z;
		break;
	case W_MINIAUDIO_ARG_OUTPUTS:
		*type = RXT_INTEGER;
		arg->uint64 = ma_node_get_output_bus_count((ma_node*)sound);
		break;
	default:
		return PE_BAD_SELECT;	
	}

	return PE_USE;
}
int MASound_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_sound* sound = (ma_sound*)hob->data;
	ma_engine* engine;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	ma_uint64 frames;
	ma_result r = MA_SUCCESS;
	ma_vec3f pos;
	ma_node *node;

	switch (word) {
	case W_MINIAUDIO_ARG_VOLUME:
		switch (*type) {
		case RXT_DECIMAL:
		case RXT_PERCENT:
			ma_sound_set_volume(sound, (float)arg->dec64);
			break;
		case RXT_INTEGER:
			ma_sound_set_volume(sound, (float)arg->int64);
			break;
		default: 
			return PE_BAD_SET_TYPE;
		}
		break;
	case W_MINIAUDIO_ARG_CURSOR:
		if (*type != RXT_INTEGER) return PE_BAD_SET_TYPE;
		if (arg->int64 < 0) arg->int64 = 0;
		ma_sound_seek_to_pcm_frame(sound, arg->uint64);
		break;
	case W_MINIAUDIO_ARG_POSITION:
		if (*type != RXT_PAIR) return PE_BAD_SET_TYPE;
		pos = ma_sound_get_position(sound);
		ma_sound_set_position(sound, arg->dec32a, arg->dec32b, pos.z);
		break;
	case W_MINIAUDIO_ARG_PAN:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_sound_set_pan(sound, (float)arg->dec64);
		break;
	case W_MINIAUDIO_ARG_PITCH:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_sound_set_pitch(sound, (float)arg->dec64);
		break;
	case W_MINIAUDIO_ARG_SPATIALIZE:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		ma_sound_set_spatialization_enabled(sound, arg->int32a);
		break;
	case W_MINIAUDIO_ARG_IS_LOOPING:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		ma_sound_set_looping(sound, arg->int32a);
		break;

	case W_MINIAUDIO_ARG_START:
	case W_MINIAUDIO_ARG_STOP:
		if (arg->int64 < 0) return PE_BAD_SET; // allow only positive time
		if   (*type == RXT_INTEGER) frames = abs_sound_frames(arg, sound);
		else if (*type == RXT_TIME) frames = abs_sound_time_to_frames(arg, sound);
		else return PE_BAD_SET_TYPE;
		if (word == W_MINIAUDIO_ARG_START)
			ma_sound_set_start_time_in_pcm_frames(sound, frames);
		else
			ma_sound_set_stop_time_in_pcm_frames(sound, frames);
		break;

	case W_MINIAUDIO_ARG_X:
	case W_MINIAUDIO_ARG_Y:
	case W_MINIAUDIO_ARG_Z:
		*type = RXT_DECIMAL;
		pos = ma_sound_get_position(sound);
		switch(word) {
		case W_MINIAUDIO_ARG_X: pos.x = (float)arg->dec64; break; 
		case W_MINIAUDIO_ARG_Y: pos.y = (float)arg->dec64; break; 
		case W_MINIAUDIO_ARG_Z: pos.z = (float)arg->dec64; break; 
		}
		ma_sound_set_position(sound, pos.x, pos.y, pos.z);

		break;

	case W_MINIAUDIO_ARG_OUTPUT:
		if (*type == RXT_NONE) {
			ma_node_detach_output_bus(sound, 0);
			break;
		}
		if (
			*type != RXT_HANDLE ||
			! arg->handle.hob   || 
			!(arg->handle.type == Handle_MADelay || arg->handle.type == Handle_MAEngine)
		) return PE_BAD_SET_TYPE;
		
		if (arg->handle.type == Handle_MADelay) {
			node = (ma_node*)arg->handle.hob->data;
		} else {
			engine = (ma_engine*)arg->handle.hob->data;
			node = ma_node_graph_get_endpoint(&engine->nodeGraph);
		}
		r = ma_node_attach_output_bus(sound, 0, node, 0);
		break;

	default:
		return PE_BAD_SET;	
	}
	if (r != MA_SUCCESS) return PE_BAD_SET;
	return PE_OK;
}


int MAGroup_free(void* hndl) {
	REBHOB *hob;
	if (!hndl) return 0;
	hob = (REBHOB *)hndl;
	debug_print("release sound group: %p is referenced: %i\n", hob->data, IS_MARK_HOB(hob) != 0);

	ma_sound_group *group = (ma_sound_group*)hob->data;

	// Don't release it, if not referenced but still playing...
	if(!IS_MARK_HOB(hob) && ma_sound_group_is_playing(group)) {
		debug_print("preventing group release?\n");
		MARK_HOB(hob);
		return 0;
	}

	UNMARK_HOB(hob);
	ma_sound_group_uninit(group);
	if (hob->series) {
		RESET_SERIES(hob->series);
		hob->series = NULL;
	}
	return 0;
}
int MAGroup_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_sound_group* group = (ma_sound_group*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	//ma_uint64 frames;
	//ma_uint32 sampleRate;
	ma_vec3f pos;

	switch (word) {
	case W_MINIAUDIO_ARG_VOLUME:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_sound_group_get_volume(group);
		break;
	case W_MINIAUDIO_ARG_POSITION:
		*type = RXT_PAIR;
		pos = ma_sound_group_get_position(group);
		arg->dec32a = pos.x;
		arg->dec32b = pos.y;
		break;
	case W_MINIAUDIO_ARG_PAN:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_sound_group_get_pan(group);
		break;
	case W_MINIAUDIO_ARG_PITCH:
		*type = RXT_DECIMAL;
		arg->dec64 = ma_sound_group_get_pitch(group);
		break;
	case W_MINIAUDIO_ARG_SAMPLE_RATE:
		*type = RXT_INTEGER;
		arg->int64 = ma_engine_get_sample_rate(ma_sound_get_engine(group));
		break;
	case W_MINIAUDIO_ARG_SPATIALIZE:
		*type = RXT_LOGIC;
		arg->int32a = ma_sound_group_is_spatialization_enabled(group);
		break;
	case W_MINIAUDIO_ARG_IS_PLAYING:
		*type = RXT_LOGIC;
		arg->int32a = ma_sound_group_is_playing(group);
		break;
	case W_MINIAUDIO_ARG_START:
		*type = RXT_INTEGER;
		arg->uint64 = ma_node_get_state_time(group, ma_node_state_started);
		break;	
	case W_MINIAUDIO_ARG_STOP:
		*type = RXT_INTEGER;
		arg->uint64 = ma_node_get_state_time(group, ma_node_state_stopped);
		break;

	case W_MINIAUDIO_ARG_X:
		*type = RXT_DECIMAL;
		pos = ma_sound_group_get_position(group);
		arg->dec64 = pos.x;
		break;
	case W_MINIAUDIO_ARG_Y:
		*type = RXT_DECIMAL;
		pos = ma_sound_group_get_position(group);
		arg->dec64 = pos.y;
		break;
	case W_MINIAUDIO_ARG_Z:
		*type = RXT_DECIMAL;
		pos = ma_sound_group_get_position(group);
		arg->dec64 = pos.z;
		break;
	case W_MINIAUDIO_ARG_OUTPUTS:
		*type = RXT_INTEGER;
		arg->uint64 = ma_node_get_output_bus_count((ma_node*)group);
		break;
	case W_MINIAUDIO_ARG_RESOURCES:
		*type = RXT_BLOCK;
		arg->series = hob->series;
		arg->index = 0;
		break;
	default:
		return PE_BAD_SELECT;	
	}

	return PE_USE;
}
int MAGroup_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_sound_group* group = (ma_sound_group*)hob->data;
	ma_engine* engine;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	ma_uint64 frames;
	ma_result r = MA_SUCCESS;
	ma_vec3f pos;
	ma_node *node;

	switch (word) {
	case W_MINIAUDIO_ARG_VOLUME:
		switch (*type) {
		case RXT_DECIMAL:
		case RXT_PERCENT:
			ma_sound_group_set_volume(group, (float)arg->dec64);
			break;
		case RXT_INTEGER:
			ma_sound_group_set_volume(group, (float)arg->int64);
			break;
		default: 
			return PE_BAD_SET_TYPE;
		}
		break;
	case W_MINIAUDIO_ARG_POSITION:
		if (*type != RXT_PAIR) return PE_BAD_SET_TYPE;
		pos = ma_sound_group_get_position(group);
		ma_sound_group_set_position(group, arg->dec32a, arg->dec32b, pos.z);
		break;
	case W_MINIAUDIO_ARG_PAN:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_sound_group_set_pan(group, (float)arg->dec64);
		break;
	case W_MINIAUDIO_ARG_PITCH:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_sound_group_set_pitch(group, (float)arg->dec64);
		break;
	case W_MINIAUDIO_ARG_SPATIALIZE:
		if (*type != RXT_LOGIC) return PE_BAD_SET_TYPE;
		ma_sound_group_set_spatialization_enabled(group, arg->int32a);
		break;

	case W_MINIAUDIO_ARG_START:
	case W_MINIAUDIO_ARG_STOP:
		if (arg->int64 < 0) return PE_BAD_SET; // allow only positive time
		if   (*type == RXT_INTEGER) frames = abs_sound_frames(arg, group);
		else if (*type == RXT_TIME) frames = abs_sound_time_to_frames(arg, group);
		else return PE_BAD_SET_TYPE;
		if (word == W_MINIAUDIO_ARG_START)
			ma_sound_group_set_start_time_in_pcm_frames(group, frames);
		else
			ma_sound_group_set_stop_time_in_pcm_frames(group, frames);
		break;

	case W_MINIAUDIO_ARG_X:
	case W_MINIAUDIO_ARG_Y:
	case W_MINIAUDIO_ARG_Z:
		*type = RXT_DECIMAL;
		pos = ma_sound_group_get_position(group);
		switch(word) {
		case W_MINIAUDIO_ARG_X: pos.x = (float)arg->dec64; break; 
		case W_MINIAUDIO_ARG_Y: pos.y = (float)arg->dec64; break; 
		case W_MINIAUDIO_ARG_Z: pos.z = (float)arg->dec64; break; 
		}
		ma_sound_group_set_position(group, pos.x, pos.y, pos.z);

		break;

	case W_MINIAUDIO_ARG_OUTPUT:
		if (*type == RXT_NONE) {
			ma_node_detach_output_bus(group, 0);
			break;
		}
		if (
			*type != RXT_HANDLE ||
			! arg->handle.hob   || 
			!(arg->handle.type == Handle_MADelay || arg->handle.type == Handle_MAEngine)
		) return PE_BAD_SET_TYPE;
		
		if (arg->handle.type == Handle_MADelay) {
			node = (ma_node*)arg->handle.hob->data;
		} else {
			engine = (ma_engine*)arg->handle.hob->data;
			node = ma_node_graph_get_endpoint(&engine->nodeGraph);
		}
		r = ma_node_attach_output_bus(group, 0, node, 0);
		break;

	default:
		return PE_BAD_SET;	
	}
	if (r != MA_SUCCESS) return PE_BAD_SET;
	return PE_OK;
}


int MANoise_free(void* hndl) {
	if (hndl != NULL) {
		debug_print("release noise: %p\n", hndl);
		ma_noise_uninit((ma_noise*)hndl, NULL);
	}
	return 0;
}
int MANoise_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_noise* noise = (ma_noise*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_AMPLITUDE:
		*type = RXT_DECIMAL;
		arg->dec64 = noise->config.amplitude;
		break;
	case W_MINIAUDIO_ARG_TYPE:
		*type = RXT_WORD;
		arg->int64 = Miniaudio_type_words[W_MINIAUDIO_TYPE_WHITE + noise->config.type];
		break;
	case W_MINIAUDIO_ARG_FORMAT:
		*type = RXT_WORD;
		arg->int64 = Miniaudio_type_words[W_MINIAUDIO_TYPE_UNKNOWN + noise->config.format];
		break;
	default:
		return PE_BAD_SELECT;	
	}
	return PE_USE;
}
int MANoise_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_noise* noise = (ma_noise*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_AMPLITUDE:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_noise_set_amplitude(noise, arg->dec64);
		break;
	default:
		return PE_BAD_SET;	
	}
	return PE_OK;
}


int MAWaveform_free(void* hndl) {
	if (hndl != NULL) {
		debug_print("release waveform: %p\n", hndl);
		ma_waveform_uninit((ma_waveform*)hndl);
	}
	return 0;
}
int MAWaveform_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_waveform* waveform = (ma_waveform*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	//printf("MAWaveform_get_path word: %u\n", word);
	switch (word) {
	case W_MINIAUDIO_ARG_AMPLITUDE:
		*type = RXT_DECIMAL;
		arg->dec64 = waveform->config.amplitude;
		break;
	case W_MINIAUDIO_ARG_FREQUENCY:
		*type = RXT_DECIMAL;
		arg->dec64 = waveform->config.frequency;
		break;
	case W_MINIAUDIO_ARG_TYPE:
		*type = RXT_WORD;
		arg->int64 = Miniaudio_type_words[W_MINIAUDIO_TYPE_SINE + waveform->config.type];
		break;
		break;
	case W_MINIAUDIO_ARG_FORMAT:
		*type = RXT_WORD;
		arg->int64 = Miniaudio_type_words[W_MINIAUDIO_TYPE_UNKNOWN + waveform->config.format];
		break;
	default:
		return PE_BAD_SELECT;	
	}
	return PE_USE;
}
int MAWaveform_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_waveform* waveform = (ma_waveform*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_AMPLITUDE:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_waveform_set_amplitude(waveform, arg->dec64);
		break;
	case W_MINIAUDIO_ARG_FREQUENCY:
		if (*type != RXT_DECIMAL) return PE_BAD_SET_TYPE;
		ma_waveform_set_frequency(waveform, arg->dec64);
		break;
	default:
		return PE_BAD_SET;	
	}
	return PE_OK;
}


int MADelay_free(void* hndl) {
	if (hndl != NULL) {
		debug_print("release delay: %p\n", hndl);
		ma_delay_node_uninit((ma_delay_node*)hndl, NULL);
	}
	return 0;
}
int MADelay_get_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_delay_node* node = (ma_delay_node*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_DELAY:
		*type = RXT_INTEGER;
		arg->uint64 = node->delay.config.delayInFrames;
		break;
	case W_MINIAUDIO_ARG_DECAY:
		*type = RXT_DECIMAL;
		arg->dec64 = node->delay.config.decay;
		break;
	case W_MINIAUDIO_ARG_DRY:
		*type = RXT_DECIMAL;
		arg->dec64 = node->delay.config.dry;
		break;
	case W_MINIAUDIO_ARG_WET:
		*type = RXT_DECIMAL;
		arg->dec64 = node->delay.config.wet;
		break;
	default:
		return PE_BAD_SELECT;	
	}
	return PE_USE;
}
int MADelay_set_path(REBHOB *hob, REBCNT word, REBCNT *type, RXIARG *arg) {
	ma_delay_node* node = (ma_delay_node*)hob->data;
	word = RL_FIND_WORD(Miniaudio_arg_words, word);
	switch (word) {
	case W_MINIAUDIO_ARG_DECAY:
		if (*type != RXT_DECIMAL && *type != RXT_PERCENT) return PE_BAD_SET_TYPE;
		ma_delay_set_decay(&node->delay, (float)arg->dec64);
		break;
	case W_MINIAUDIO_ARG_DRY:
		if (*type != RXT_DECIMAL && *type != RXT_PERCENT) return PE_BAD_SET_TYPE;
		ma_delay_set_dry(&node->delay, (float)arg->dec64);
		break;
	case W_MINIAUDIO_ARG_WET:
		if (*type != RXT_DECIMAL && *type != RXT_PERCENT) return PE_BAD_SET_TYPE;
		ma_delay_set_wet(&node->delay, (float)arg->dec64);
		break;
	default:
		return PE_BAD_SET;	
	}
	return PE_OK;
}



COMMAND cmd_miniaudio_version(RXIFRM *frm, void *ctx) {
	RXA_TYPE(frm, 1) = RXT_TUPLE;
	RXA_TUPLE(frm, 1)[0] = MA_VERSION_MAJOR;
	RXA_TUPLE(frm, 1)[1] = MA_VERSION_MINOR;
	RXA_TUPLE(frm, 1)[2] = MA_VERSION_REVISION;
	RXA_TUPLE_LEN(frm, 1) = 3;
	return RXR_VALUE;
}


COMMAND cmd_miniaudio_get_devices(RXIFRM *frm, void *ctx) {
	ASSERT_CONTEXT();

	ma_device_info* pPlaybackDeviceInfos;
	ma_uint32 playbackDeviceCount;
	ma_device_info* pCaptureDeviceInfos;
	ma_uint32 captureDeviceCount;
	ma_uint32 iDevice, iData;

	if (MA_SUCCESS != ma_context_get_devices(&gContext, &pPlaybackDeviceInfos, &playbackDeviceCount, &pCaptureDeviceInfos, &captureDeviceCount)) {
		RETURN_ERROR("Failed to retrieve device information.");
	}
	REBLEN len;
	REBSER *str = RL_MAKE_STRING(1000, FALSE); // temporary 1024 bytes.. can be extended automatically
	RXA_TYPE(frm, 1) = RXT_STRING;
	RXA_INDEX(frm, 1) = 0;

	APPEND_STRING(str, "\nplayback:  [\n");

	//printf("Playback Devices\n");
	for (iDevice = 0; iDevice < playbackDeviceCount; ++iDevice) {
		//printf("    %u: %s %u\n", iDevice, pPlaybackDeviceInfos[iDevice].name, pPlaybackDeviceInfos[iDevice].nativeDataFormatCount);
		APPEND_STRING(str, " \"%s\"\n", pPlaybackDeviceInfos[iDevice].name);
		for (iData = 0; iData < pPlaybackDeviceInfos[iDevice].nativeDataFormatCount; ++iData) {
		//	printf("        %u\n",          pPlaybackDeviceInfos[iDevice].nativeDataFormats[iData].sampleRate);
		}
	}	

	//printf("\n");

	APPEND_STRING(str, "]\ncapture: [\n");
	//printf("Capture Devices\n");
	for (iDevice = 0; iDevice < captureDeviceCount; ++iDevice) {
		APPEND_STRING(str, " \"%s\"\n", pCaptureDeviceInfos[iDevice].name);
		//printf("    %u: %s\n", iDevice, pCaptureDeviceInfos[iDevice].name);
	}

	APPEND_STRING(str, "]\n");

	// Convert UTF-8 output to Rebol string...
	RXA_SERIES(frm, 1) = RL_DECODE_UTF_STRING(SERIES_DATA(str), SERIES_TAIL(str), 8, 0, 0);

	return RXR_VALUE;
}

COMMAND cmd_miniaudio_init_playback(RXIFRM *frm, void *ctx) {
	ASSERT_CONTEXT();

	REBHOB* hob;
	ma_result result;
	ma_device_info* pPlaybackDeviceInfos;
	ma_uint32 playbackDeviceCount;
	ma_uint32 iChosenDevice;
	ma_device_config deviceConfig;
	ma_engine_config engineConfig;
	MAContext *context;

	result = ma_context_get_devices(&gContext, &pPlaybackDeviceInfos, &playbackDeviceCount, NULL, NULL);
	if (result != MA_SUCCESS) {
		RETURN_ERROR("Failed to enumerate playback devices.");
	}

	iChosenDevice = (ma_uint32)(RXA_INT64(frm, 1) - 1);
	if (iChosenDevice < 0 || iChosenDevice >= playbackDeviceCount) {
		RETURN_ERROR("Invalid device index value.");
	}

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_MAEngine);
	if (hob == NULL) return RXR_NONE;
	context = (MAContext*)hob->data;
	context->engine = malloc(sizeof(ma_engine));
	context->device = malloc(sizeof(ma_device));

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.periodSizeInMilliseconds = (ma_uint32)RXA_UINT64(frm, 6);;
	deviceConfig.playback.pDeviceID = &pPlaybackDeviceInfos[iChosenDevice].id;
	deviceConfig.playback.format    = gResourceManager.config.decodedFormat;
	deviceConfig.playback.channels  = (ma_uint32)RXA_INT64(frm, 4);
	deviceConfig.sampleRate         = gResourceManager.config.decodedSampleRate;
	deviceConfig.dataCallback       = data_callback;
	deviceConfig.pUserData          = context;

	result = ma_device_init(&gContext, &deviceConfig, context->device);
	if (result != MA_SUCCESS) {
		debug_print("Failed to initialize device for %s.\n", pPlaybackDeviceInfos[iChosenDevice].name);
		RETURN_ERROR("Failed to initialize device.");
	}

	engineConfig = ma_engine_config_init();
	engineConfig.pDevice          = context->device;
	engineConfig.pResourceManager = &gResourceManager;
	engineConfig.noAutoStart      = RXA_REF(frm, 2);

	result = ma_engine_init(&engineConfig, context->engine);
	if (result != MA_SUCCESS) {
		debug_print("Failed to initialize engine for %s.\n", pPlaybackDeviceInfos[iChosenDevice].name);
		ma_device_uninit(context->device);
		RETURN_ERROR("Failed to initialize engine.");
	}

	pEngine = context;
	pEngineHob = hob;
	hob->series = RL_MAKE_BLOCK(10); // for keeping references to sound handles

	if (RXA_REF(frm, 7)) {
		// on-data callback
		CLEAR(&context->callback, sizeof(context->callback));
		context->callback.obj  = RXA_OBJECT(frm, 8);
		context->callback.word = RXA_WORD(frm, 9);
	}

	RETURN_HANDLE(hob);
}


COMMAND cmd_miniaudio_play(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	ma_sound_group *group = NULL;
	ma_uint32 flags = 0;
	REBHOB* hob = NULL;
	REBI64 frames;
	REBSER* ser;
	REBSER* blk;

	ASSERT_ENGINE();

	if (RXA_REF(frm, 8)) {
		group = ARG_MAGroup(9);
		if (!group) RETURN_ERROR("Invalid group handle.");
	}

	if (RXA_TYPE(frm, 1) == RXT_FILE) {
		hob = RL_MAKE_HANDLE_CONTEXT(Handle_MASound);
		if (hob == NULL) return RXR_NONE;
		sound = (ma_sound*)hob->data;

		if (RXA_REF(frm, 2)) flags |= MA_SOUND_FLAG_STREAM;
		if (MA_SUCCESS != sound_init_from_arg(&RXA_ARG(frm, 1), &ser, flags, group, NULL, sound))
			RETURN_ERROR("Failed to initialize the sound from file.");

		// store the full path of the source file in the handle
		hob->series = RL_MAKE_BLOCK(1);
		RXA_SERIES(frm, 1) = ser;
		RXA_INDEX(frm, 1) = 0;
		RL_SET_VALUE(hob->series, 0, RXA_ARG(frm, 1), RXT_STRING);

	} else if (ARG_Is_MASound(1)) {
		sound = ARG_MASound(1);
	} else if (ARG_Is_DataSource(1)) {
		ma_data_source *source = ARG_DataSource(1);

		hob = RL_MAKE_HANDLE_CONTEXT(Handle_MASound);
		if (hob == NULL) return RXR_NONE;
		sound = (ma_sound*)hob->data;
		if (MA_SUCCESS != ma_sound_init_from_data_source(pEngine->engine, source, 0, group, sound))
			RETURN_ERROR("Failed to initialize the sound from a data source.");

		// store the datasource in the sound, so it it markable from GC
		hob->series = RL_MAKE_BLOCK(1);
		RL_SET_VALUE(hob->series, 0, RXA_ARG(frm, 1), RXT_HANDLE);
	} else {
		return RXR_NONE;
	}

	if (RXA_REF(frm, 4)) ma_sound_set_volume(sound, ARG_Float(5));
	if (MA_SUCCESS != ma_sound_start(sound)) RETURN_ERROR("Failed to start sound");
	ma_sound_set_looping(sound, RXA_REF(frm, 3));

	if (RXA_REF(frm, 6)) { // fade
		if (RXA_TYPE(frm, 7) == RXT_INTEGER) {
			frames = RXA_INT64(frm, 7);
		} else {
			ma_uint64 sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
			frames = (RXA_TIME(frm, 7) * sampleRate) / 1000000000;
		}
		if (frames > 0) {
			ma_sound_set_fade_in_pcm_frames(sound, 0, 1, frames);
		}
	}

	if (hob) {
		RXA_HANDLE(frm, 1)       = hob;
		RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
		RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
		RXA_TYPE(frm, 1) = RXT_HANDLE;

		blk = pEngineHob->series;
		RL_SET_VALUE(blk, blk->tail, RXA_ARG(frm, 1), RXT_HANDLE);

		if (group) {
			// store the sound reference also in the group
			blk = ARG_Handle_Series(9);
			RL_SET_VALUE(blk, blk->tail, RXA_ARG(frm, 1), RXT_HANDLE);
		}
	}


	return RXR_VALUE;
}

COMMAND cmd_miniaudio_pause(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	
	sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;

	ma_sound_stop(sound);
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_start(RXIFRM *frm, void *ctx) {
	ma_sound  *sound;
	MAContext *context;
	ma_uint64 sampleRate;
	ma_uint64 frame = 0;
	REBHOB  *hob = RXA_HANDLE_CONTEXT(frm, 1);
	if (!IS_USED_HOB(hob)) return RXR_FALSE; // already released handle!

	ASSERT_ENGINE();

	if (RXA_HANDLE_TYPE(frm, 1) == Handle_MASound) {
		sound = ARG_MASound(1);
		if (sound == NULL) return RXR_FALSE;

		ma_sound_set_looping(sound, RXA_REF(frm, 2));

		if (RXA_REF(frm, 3)) {
			if (RXA_INT64(frm, 4) < 0) RXA_INT64(frm, 4) = 0;
			if (RXA_TYPE(frm, 4) == RXT_INTEGER) {
				frame = RXA_UINT64(frm, 4);
			} else {
				sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
				frame = (RXA_TIME(frm, 4) * sampleRate) / 1000000000; // rate is per second
			}
		}
		ma_sound_seek_to_pcm_frame(sound, frame);
		// If the stop time is in the past, reset it to the future; otherwise, the sound will not play again.
		// See: https://github.com/mackron/miniaudio/issues/714
		if (ma_node_get_state_time((ma_node*)sound, ma_node_state_stopped) <= ma_engine_get_time_in_pcm_frames(pEngine->engine))
			ma_sound_set_stop_time_in_pcm_frames(sound, ~(ma_uint64)0);

		if (RXA_REF(frm, 7)) { // /at
			if (RXA_INT64(frm, 8) < 0) RXA_INT64(frm, 8) = 0;
			if (RXA_TYPE(frm, 8) == RXT_INTEGER) {
				frame = RXA_UINT64(frm, 8);
			} else {
				sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
				frame = (RXA_TIME(frm, 8) * sampleRate) / 1000000000; // rate is per second
			}
			ma_sound_set_start_time_in_pcm_frames(sound, frame);
		}

		ma_sound_start(sound);

		if (RXA_REF(frm, 5)) { // /fade
			ma_uint64 fade;
			switch(RXA_TYPE(frm, 6)) {
			case RXT_INTEGER:
				fade = (ma_uint64)RXA_INT64(frm, 6);
				if (fade > 0) ma_sound_set_fade_in_pcm_frames(sound, 0, 1, fade);
				break;
			case RXT_TIME:
				fade = RXA_TIME(frm, 6) / 1000000; // time in ms
				if (fade > 0) ma_sound_set_fade_in_milliseconds(sound, 0, 1, fade);
				break;
			}
		}
		return RXR_VALUE;
	}
	else if (RXA_HANDLE_TYPE(frm, 1) == Handle_MAEngine) {
		context = (MAContext*)hob->handle;
		if (RXA_REF(frm, 3)) {
			if (RXA_INT64(frm, 4) < 0) RXA_INT64(frm, 4) = 0; // only positive values
			if (RXA_TYPE(frm, 4) == RXT_INTEGER) {
				frame = RXA_INT64(frm, 4);
			} else {
				sampleRate = ma_engine_get_sample_rate(context->engine);
				frame = (RXA_TIME(frm, 4) * sampleRate) / 1000000000; // rate is per second
			}
			ma_engine_set_time_in_pcm_frames(context->engine, (ma_uint64)frame);
		}
		ma_device_start(context->device);
		return RXR_VALUE;
	}
	return RXR_FALSE;
}

COMMAND cmd_miniaudio_stop(RXIFRM *frm, void *ctx) {
	ma_sound  *sound;
	MAContext *context;
	REBI64 fade;
	REBHOB  *hob = RXA_HANDLE_CONTEXT(frm, 1);
	if (!IS_USED_HOB(hob)) return RXR_FALSE; // already released handle!

	if (RXA_HANDLE_TYPE(frm, 1) == Handle_MASound) {
		sound = (ma_sound*)hob->handle;
		switch(RXA_TYPE(frm, 3)) {
		case RXT_NONE: ma_sound_stop(sound); break;
		case RXT_INTEGER: ma_sound_stop_with_fade_in_pcm_frames(sound, (ma_uint64)RXA_INT64(frm, 3)); break;
		case RXT_TIME:
			fade = RXA_TIME(frm, 3) / 1000000; // time in ms
			if (fade <= 0) ma_sound_stop(sound);
			ma_sound_stop_with_fade_in_milliseconds(sound, (ma_uint64)fade);
			break;
		}
		return RXR_VALUE;
	}
	else if (RXA_HANDLE_TYPE(frm, 1) == Handle_MAEngine) {
		context = (MAContext*)hob->handle;
		ma_device_stop(context->device);
		return RXR_VALUE;
	}
	return RXR_FALSE;
}

COMMAND cmd_miniaudio_fade(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	REBI64 frames;
	float volumeBeg, volumeEnd;

	sound  = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;

	if (RXA_TYPE(frm, 2) == RXT_INTEGER) {
		frames = RXA_INT64(frm, 2);
	} else {
		ma_uint64 sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
		frames = (RXA_TIME(frm, 2) * sampleRate) / 1000000000;
	}

	volumeBeg = ARG_Float(3);
	volumeEnd = ARG_Float(4);

	if (frames < 0) frames = 1;
	//if (volumeBeg == 0.0) ma_sound_set_volume(sound, 0.01);
	//printf("fade from %f to %f in %llu\n", ma_sound_get_volume(sound), volumeEnd, frames);
	ma_sound_set_fade_in_pcm_frames(sound, volumeBeg, volumeEnd, frames);

	return RXR_VALUE;
}

COMMAND cmd_miniaudio_seek(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	ma_uint64 cursor = 0;
	REBI64 frame;

	sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;

	if (RXA_TYPE(frm, 2) == RXT_INTEGER) {
		frame = RXA_INT64(frm, 2);
	} else {
		ma_uint64 sampleRate = ma_engine_get_sample_rate(ma_sound_get_engine(sound));
		frame = (RXA_TIME(frm, 2) * sampleRate) / 1000000000;
	}
	
	if (RXA_REF(frm, 3)) {
		// relative
		ma_sound_get_cursor_in_pcm_frames(sound, &cursor);
		frame += cursor;
	}
	if (frame < 0) frame = 0;

	ma_sound_seek_to_pcm_frame(sound, frame);
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_load(RXIFRM *frm, void *ctx) {
	ma_result result;
	ma_sound *sound;
	ma_sound_group *group = NULL;
	REBHOB* hob;
	REBSER* ser;
	REBSER* blk;

	ASSERT_ENGINE();

	if (RXA_REF(frm, 2)) {
		group = ARG_MAGroup(3);
		if (!group) RETURN_ERROR("Invalid group handle.");
	}

	hob = RL_MAKE_HANDLE_CONTEXT(Handle_MASound);
	if (hob == NULL) return RXR_NONE;
	sound = (ma_sound*)hob->data;

	result = sound_init_from_arg(&RXA_ARG(frm, 1), &ser, 0, group, NULL, sound);	
	if (result != MA_SUCCESS) RETURN_ERROR("Failed to initialize the sound from a file.");


	// store the full path of the source file in the handle
	hob->series = RL_MAKE_BLOCK(1);
	RXA_SERIES(frm, 1) = ser;
	RXA_INDEX(frm, 1) = 0;
	RL_SET_VALUE(hob->series, 0, RXA_ARG(frm, 1), RXT_STRING);

	RXA_HANDLE(frm, 1)       = hob;
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;

	blk = pEngineHob->series;
	RL_SET_VALUE(blk, blk->tail, RXA_ARG(frm, 1), RXT_HANDLE);

	if (group) {
		// store the sound reference also in the group
		blk = ARG_Handle_Series(3);
		RL_SET_VALUE(blk, blk->tail, RXA_ARG(frm, 1), RXT_HANDLE);
	}

	return RXR_VALUE;
}


COMMAND cmd_miniaudio_make_noise_node(RXIFRM *frm, void *ctx) {
	ma_noise *noise;
	REBHOB* hob = RL_MAKE_HANDLE_CONTEXT(Handle_MANoise);
	if (hob == NULL) return RXR_NONE;
	noise = (ma_noise*)hob->data;

	REBCNT format = ARG_Int32(6);
	if (!format || format >= ma_format_count) format = ma_format_s16;

	ma_noise_config config = ma_noise_config_init(
		format,        //= format
		1,
		ARG_Int32(1),  //= type
		ARG_Int32(4),  //= seed
		ARG_Double(2)  //= apmplitude
	);

	if (MA_SUCCESS != ma_noise_init(&config, NULL, noise)) return RXR_FALSE;

	hob->series = NULL;
	RXA_HANDLE(frm, 1)       = hob;
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;

	return RXR_VALUE;
}

COMMAND cmd_miniaudio_make_waveform_node(RXIFRM *frm, void *ctx) {
	ma_waveform *waveform;

	ASSERT_ENGINE();

	REBHOB* hob = RL_MAKE_HANDLE_CONTEXT(Handle_MAWaveform);
	if (hob == NULL) return RXR_NONE;
	waveform = (ma_waveform*)hob->data;	

	REBCNT format = ARG_Int32(5);
	if (!format || format >= ma_format_count) format = ma_format_s16;

	ma_waveform_config config = ma_waveform_config_init(
		format,        //= format
		1,             //= channels
		ma_engine_get_sample_rate(pEngine->engine),
		ARG_Int32(1),  //= type
		ARG_Double(2), //= apmplitude
		ARG_Double(3)  //= frequency
	);

	if (MA_SUCCESS != ma_waveform_init(&config, waveform)) return RXR_FALSE;

	hob->series = NULL;
	RXA_HANDLE(frm, 1)       = hob;
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;

	return RXR_VALUE;
}

COMMAND cmd_miniaudio_make_delay_node(RXIFRM *frm, void *ctx) {
	ma_delay_node *delay;
	ma_uint32 channels;
	ma_uint32 sampleRate;
	ma_uint32 delayInFrames;
	float decay;

	ASSERT_ENGINE();

	REBHOB* hob = RL_MAKE_HANDLE_CONTEXT(Handle_MADelay);
	if (hob == NULL) return RXR_NONE;
	delay = (ma_delay_node*)hob->data;

	channels   = ma_engine_get_channels(pEngine->engine);
	sampleRate = ma_engine_get_sample_rate(pEngine->engine);

	if (RXA_INT64(frm, 1) < 0) RXA_INT64(frm, 1) = 1;
	else if (RXA_TYPE(frm, 1) == RXT_INTEGER) {
		delayInFrames = (ma_uint32)RXA_UINT64(frm, 1);
	} else if (RXA_TYPE(frm, 1) == RXT_DECIMAL) {
		delayInFrames = (ma_uint32)(RXA_DEC64(frm, 1) * sampleRate);
	} else {
		delayInFrames = (ma_uint32)((RXA_TIME(frm, 1) * sampleRate) / 1000000000);
	}

	decay = (float)fmax(0.0, fmin(1.0, ARG_Double(2)));

	ma_delay_node_config config = ma_delay_node_config_init(channels, sampleRate, delayInFrames, decay);

	if (RXA_REF(frm, 3)) config.delay.dry = (float)fmax(0.0, fmin(1.0, ARG_Double(4)));
	if (RXA_REF(frm, 5)) config.delay.wet = (float)fmax(0.0, fmin(1.0, ARG_Double(6)));


	if (MA_SUCCESS != ma_delay_node_init(ma_engine_get_node_graph(pEngine->engine), &config, NULL, delay))
		RETURN_ERROR("Failed to initialize the delay node.");

	/* Connect the output of the delay node to the input of the endpoint. */
	ma_node_attach_output_bus(delay, 0, ma_engine_get_endpoint(pEngine->engine), 0);

	hob->series = NULL;
	RXA_HANDLE(frm, 1)       = hob;
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;

	/* Keep the reference to the handle so it is not released by GC */
	REBSER *blk = pEngineHob->series;
	RL_SET_VALUE(blk, blk->tail, RXA_ARG(frm, 1), RXT_HANDLE);

	return RXR_VALUE;
}

COMMAND cmd_miniaudio_make_group_node(RXIFRM *frm, void *ctx) {
	ma_sound_group *group;

	ASSERT_ENGINE();

	REBHOB* hob = RL_MAKE_HANDLE_CONTEXT(Handle_MAGroup);
	if (hob == NULL) return RXR_NONE;
	group = (ma_sound_group*)hob->data;

	if (MA_SUCCESS != ma_sound_group_init(pEngine->engine, 0, NULL, group))
		RETURN_ERROR("Failed to initialize the sound group.");

	hob->series = RL_MAKE_BLOCK(1);
	RXA_HANDLE(frm, 1)       = hob;
	RXA_HANDLE_TYPE(frm, 1)  = hob->sym;
	RXA_HANDLE_FLAGS(frm, 1) = hob->flags;
	RXA_TYPE(frm, 1) = RXT_HANDLE;

	/* Keep the reference to the handle so it is not released by GC */
	REBSER *blk = pEngineHob->series;
	RL_SET_VALUE(blk, blk->tail, RXA_ARG(frm, 1), RXT_HANDLE);

	return RXR_VALUE;
}




COMMAND cmd_miniaudio_volume(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	float volume;
	
	sound = ARG_MASound(1);
	volume = ARG_Float(2);

	if (sound == NULL) return RXR_FALSE;
	ma_sound_set_volume(sound, volume);
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_volumeq(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	
	sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;

	RXA_DEC64(frm, 1) = ma_sound_get_volume(sound);
	RXA_TYPE(frm, 1) = RXT_DECIMAL;
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_pan(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	float pan;
	
	sound = ARG_MASound(1);
	pan   = ARG_Float(2);

	if (sound == NULL) return RXR_FALSE;
	ma_sound_set_pan(sound, pan);
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_panq(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	
	sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;

	RXA_DEC64(frm, 1) = ma_sound_get_pan(sound);
	RXA_TYPE(frm, 1) = RXT_DECIMAL;
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_pitch(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	float pitch;
	
	sound = ARG_MASound(1);
	pitch   = ARG_Float(2);

	if (sound == NULL) return RXR_FALSE;
	ma_sound_set_pitch(sound, pitch);
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_pitchq(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	
	sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;

	RXA_DEC64(frm, 1) = ma_sound_get_pitch(sound);
	RXA_TYPE(frm, 1) = RXT_DECIMAL;
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_looping(RXIFRM *frm, void *ctx) {
	ma_sound *sound;
	
	sound = ARG_MASound(1);

	if (sound == NULL) return RXR_FALSE;
	ma_sound_set_looping(sound, RXA_LOGIC(frm,2));
	return RXR_VALUE;
}

COMMAND cmd_miniaudio_loopingq(RXIFRM *frm, void *ctx) {
	ma_sound *sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;
	return ma_sound_is_looping(sound) ? RXR_TRUE : RXR_FALSE;
}

COMMAND cmd_miniaudio_endq(RXIFRM *frm, void *ctx) {
	ma_sound *sound = ARG_MASound(1);
	if (sound == NULL) return RXR_FALSE;
	return ma_sound_at_end(sound) ? RXR_TRUE : RXR_FALSE;
}





